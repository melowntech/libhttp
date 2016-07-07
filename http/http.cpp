#include <ctime>
#include <algorithm>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <memory>

#include <boost/noncopyable.hpp>
#include <boost/asio.hpp>
#include <boost/format.hpp>

#include <arpa/inet.h>

#include "dbglog/dbglog.hpp"

#include "utility/raise.hpp"
#include "utility/gccversion.hpp"
#include "utility/streams.hpp"
#include "utility/buildsys.hpp"

#include "./error.hpp"
#include "./http.hpp"
#include "./detail/detail.hpp"
#include "./detail/types.hpp"
#include "./detail/serverconnection.hpp"
#include "./detail/acceptor.hpp"

namespace http {

namespace {

const std::string error400(R"RAW(<html>
<head><title>400 Bad Request</title></head>
<body bgcolor="white">
<center><h1>400 Bad Request</h1></center>
)RAW");

const std::string error404(R"RAW(<html>
<head><title>404 Not Found</title></head>
<body bgcolor="white">
<center><h1>404 Not Found</h1></center>
)RAW");

const std::string error500(R"RAW(<html>
<head><title>500 Internal Server Error</title></head>
<body bgcolor="white">
<center><h1>500 Internal Server Error</h1></center>
)RAW");

const std::string error503(R"RAW(<html>
<head><title>503 Service Temporarily Unavailable</title></head>
<body bgcolor="white">
<center><h1>503 Service Temporarily Unavailable</h1></center>
)RAW");

const std::string error405(R"RAW(<html>
<head><title>405 Method Not Allowed</title></head>
<body bgcolor="white">
<center><h1>405 Method Not Allowed</h1></center>
)RAW");

const char *weekDays[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

std::string formatHttpDate(time_t time)
{
    if (time < 0) { time = std::time(nullptr); }
    tm bd;
    ::gmtime_r(&time, &bd);
    char buf[32];
    std::memcpy(buf, weekDays[bd.tm_wday], 3);
    std::strftime(buf + 3, sizeof(buf) - 1
                  , ", %d %b %Y %H:%M:%S GMT", &bd);
    return buf;
}

} // namespace

namespace detail {

std::atomic<std::size_t> ServerConnection::idGenerator_(0);

} // namespace detail


Http::Detail::Detail()
    : work_(std::ref(ios_))
    , resolver_(ios_)
    , running_(false)
{}

void Http::Detail::start(std::size_t count)
{
    if (running_) {
        LOGTHROW(err3, Error) << "HTTP machinery is already running.";
    }

    // make sure threads are released when something goes wrong
    struct Guard {
        Guard(const std::function<void()> &func) : func(func) {}
        ~Guard() { if (func) { func(); } }
        void release() { func = {}; }
        std::function<void()> func;
    } guard([this]() { stop(); });

    for (std::size_t id(1); id <= count; ++id) {
        workers_.emplace_back(&Detail::worker, this, id);
    }

    guard.release();
    running_ = true;
}

void Http::Detail::stop()
{
    LOG(info2) << "Stopping HTTP.";
    {
        std::unique_lock<std::mutex> lock(connMutex_);

        // get rid of all acceptors
        acceptors_.clear();

        for (const auto &item : connections_) {
            item->close();
        }

        while (!connections_.empty()) {
            connCond_.wait(lock);
        }
    }

    work_ = boost::none;

    while (!workers_.empty()) {
        workers_.back().join();
        workers_.pop_back();
    }

    running_ = false;
}

void Http::Detail::listen(const utility::TcpEndpoint &listen
                          , const ContentGenerator::pointer &contentGenerator)
{
    std::unique_lock<std::mutex> lock(connMutex_);

    acceptors_.push_back(std::make_shared<detail::Acceptor>
                         (*this, ios_, listen, contentGenerator));
}

void Http::Detail::worker(std::size_t id)
{
    dbglog::thread_id(str(boost::format("http:%u") % id));
    LOG(info2) << "Spawned HTTP worker id:" << id << ".";

    for (;;) {
        try {
            ios_.run();
            LOG(info2) << "Terminated HTTP worker id:" << id << ".";
            return;
        } catch (const std::exception &e) {
            LOG(err3)
                << "Uncaught exception in HTTP worker: <" << e.what()
                << ">. Going on.";
        }
    }
}

void Http::Detail
::addServerConnection(const detail::ServerConnection::pointer &conn)
{
    std::unique_lock<std::mutex> lock(connMutex_);
    connections_.insert(conn);
}

void Http::Detail
::removeServerConnection(const detail::ServerConnection::pointer &conn)
{
    {
        std::unique_lock<std::mutex> lock(connMutex_);
        connections_.erase(conn);
    }
    connCond_.notify_one();
}

namespace detail {

void Acceptor::start()
{
    auto conn(std::make_shared<ServerConnection>
              (owner_, ios_, contentGenerator_));

    acceptor_.async_accept(conn->socket()
                           , [=](const bs::error_code &ec)
    {
        if (!ec) {
            owner_.addServerConnection(conn);
            conn->start();
        } else if (ec == asio::error::operation_aborted) {
            // aborted -> closing shop
            return;
        } else {
            LOG(err2) << "error accepting: " << ec;
        }

        start();
    });
}

void prelogAndProcess(Http::Detail &detail
                      , const ServerConnection::pointer &connection
                      , const Request &request)
{
    LOG(info2, connection->lm())
        << "HTTP \"" << request.method << ' ' << request.uri
        << ' ' << request.version << "\".";
    detail.request(connection, request);
}

void postLog(const ServerConnection::pointer &connection
             , const Request &request, const Response &response
             , std::size_t size)
{
    if (response.code == StatusCode::OK) {
        LOG(info3, connection->lm())
            << "HTTP \"" << request.method << ' ' << request.uri
            << ' ' << request.version << "\" " << response.numericCode()
            << ' ' << size << '.';
        return;
    }

    LOG(info3, connection->lm())
        << "HTTP \"" << request.method << ' ' << request.uri
        << ' ' << request.version << "\" " << response.numericCode()
        << ' ' << size << " [" << response.reason << "].";
}

void ServerConnection::setAborter(const ServerSink::AbortedCallback &ac)
{
    std::unique_lock<std::mutex> lock(acMutex_);
    ac_ = ac;
}

void ServerConnection::aborted()
{
    // grab current value of callback
    auto ac([&]() -> ServerSink::AbortedCallback
    {
        std::unique_lock<std::mutex> lock(acMutex_);
        return ac_;
    }());

    // call it without locking
    if (ac) { ac(); }
}

bool ServerConnection::finished() const
{
    switch (state_) {
    case State::ready: case State::busy: return false;
    case State::busyClose: case State::closed: return true;
    }
    return false;
}

void ServerConnection::process()
{
    switch (state_) {
    case State::busy:
    case State::busyClose:
        return;


    case State::closed:
        return;

    case State::ready:
        // about to try to run a request
        break;
    }

    // try request
    switch (requests_.front().state) {
    case Request::State::ready:
        state_ = State::busy;
        prelogAndProcess(owner_, shared_from_this(), pop());
        break;

    case Request::State::broken:
        badRequest();
        break;

    default:
        // nothing to do now
        break;
    }
}

void ServerConnection::makeReady()
{
    switch (state_) {
    case State::busy:
        state_ = State::ready;
        break;

    case State::busyClose:
        close();
        break;

    case State::closed:
    case State::ready:
        break;
    }
}

void ServerConnection::close(const bs::error_code &ec)
{
    if ((ec == asio::error::misc_errors::eof)
        || (ec == asio::error::operation_aborted)
        || (ec == asio::error::connection_reset))
    {
        LOG(info1, lm_) << "ServerConnection closed.";
    } else {
        LOG(err2, lm_) << "Error: " << ec;
        bs::error_code cec;
        socket_.close(cec);
    }

    // aborted
    state_ = State::closed;
    aborted();
    owner_.removeServerConnection(shared_from_this());
}

void ServerConnection::close()
{
    if (state_ != State::closed) {
        LOG(info2, lm_) << "ServerConnection closed.";
        bs::error_code cec;
        socket_.close(cec);
    }
}

bool ServerConnection::valid() const
{
    switch (state_) {
    case State::closed:
    case State::busyClose:
        return false;

    default: break;
    }
    return true;
}

void ServerConnection::start()
{
    LOG(info1, lm_) << "ServerConnection opened.";
    requests_.emplace_back();
    readRequest();
}

void ServerConnection::readRequest()
{
    auto self(shared_from_this());

    auto parseRequest([self, this](const bs::error_code &ec
                                   , std::size_t bytes)
    {
        auto &request(requests_.back());

        if (ec) {
            // handle error
            close(ec);
            return;
        }

        ++request.lines;

        if (bytes == 2) {
            // empty line -> restart request parsing
            requestData_.consume(bytes);
            readRequest();
            return;
        }

        std::istream is(&requestData_);
        is.unsetf(std::ios_base::skipws);
        if (!(is >> request.method >> utility::expect(' ')
              >> request.uri >> utility::expect(' ')
              >> request.version
              >> utility::expect('\r') >> utility::expect('\n')))
        {
            request.makeBroken();
            process();
            return;
        }
        readHeader();
    });

    requests_.back().clear();
    asio::async_read_until(socket_, requestData_, "\r\n"
                           , strand_.wrap(parseRequest));
}

void ServerConnection::readHeader()
{
    auto self(shared_from_this());

    auto parseHeader([self, this](const bs::error_code &ec
                                  , std::size_t bytes)
    {
        auto &request(requests_.back());

        if (ec) {
            // handle error
            close(ec);
            return;
        }

        ++request.lines;

        if (bytes == 2) {
            // empty line -> restart request parsing
            requestData_.consume(bytes);
            request.makeReady();

            // try to process immediately
            process();

            // and start again
            start();
            return;
        }

        std::istream is(&requestData_);
        if (std::isspace(is.peek())) {
            if (request.headers.empty()) {
                request.makeBroken();
                process();
                return;
            }

            auto &header(request.headers.back());
            if (!std::getline(is, header.value, '\r')) {
                request.makeBroken();
                process();
                return;
            }
            // eat '\n'
            is.get();
            readHeader();
            return;
        }

        request.headers.emplace_back();
        auto &header(request.headers.back());

        if (!std::getline(is, header.name, ':')) {
            request.makeBroken();
            process();
            return;
        }
        if (!std::getline(is, header.value, '\r')) {
            request.makeBroken();
            process();
            return;
        }
        // eat '\n'
        is.get();

        readHeader();
    });

    asio::async_read_until(socket_, requestData_, "\r\n"
                           , strand_.wrap(parseHeader));
}

void ServerConnection::badRequest()
{
    Response response(StatusCode::BadRequest);
    response.close = true;
    response.reason = "Bad request";

    LOG(debug) << "About to send http error: <" << response.code << ">.";

    response.headers.emplace_back("Content-Type", "text/html; charset=utf-8");

    sendResponse({}, response, error400, true);
}

void ServerConnection::sendResponse(const Request &request, const Response &response
                              , const void *data, const size_t size
                              , bool persistent)
{
    std::ostream os(&responseData_);

    os << request.version << ' ' << response.numericCode() << ' '
       << response.code << "\r\n";

    os << "Date: " << formatHttpDate(-1) << "\r\n";
    os << "Server: " << ubs::TargetName << '/' << ubs::TargetVersion << "\r\n";
    for (const auto &hdr : response.headers) {
        os << hdr.name << ": "  << hdr.value << "\r\n";
    }

    // optional data
    if (data) {
        os << "Content-Length: " << size << "\r\n";
    } else {
        os << "Content-Length: 0\r\n";
    }
    if (response.close) { os << "ServerConnection: close\r\n"; }

    os << "\r\n";

    if (request.method == "HEAD") {
        // HEAD request -> send no data
        data = nullptr;
    }

    if (!persistent && data) {
        os.write(static_cast<const char*>(data), size);
    }

    // mark as busy/close
    if (response.close) {
        state_ = State::busyClose;
    }

    auto self(shared_from_this());
    auto responseSent([self, this, request, response]
                      (const bs::error_code &ec, std::size_t bytes)
    {
        if (ec) {
            close(ec);
            return;
        }

        // eat data from response
        responseData_.consume(bytes);

        // log what happened
        postLog(self, request, response, bytes);

        // response sent, not busy for now
        makeReady();

        // we are not busy so try next request immediately
        process();
    });

    if (persistent && data) {
        std::vector<asio::const_buffer> buffers = {
            responseData_.data()
            , asio::const_buffer(data, size)
        };

        asio::async_write(socket_, buffers
                          , strand_.wrap(responseSent));
    } else {
        asio::async_write(socket_, responseData_.data()
                          , strand_.wrap(responseSent));
    }
}

void ServerConnection
::sendResponse(const Request &request, const Response &response
               , const SinkBase::DataSource::pointer &source)
{
    std::ostream os(&responseData_);

    os << request.version << ' ' << response.numericCode() << ' '
       << response.code << "\r\n";

    os << "Date: " << formatHttpDate(-1) << "\r\n";
    os << "Server: " << ubs::TargetName << '/' << ubs::TargetVersion << "\r\n";
    for (const auto &hdr : response.headers) {
        os << hdr.name << ": "  << hdr.value << "\r\n";
    }

    // size
    auto stat(source->stat());
    os << "Content-Type: " << stat.contentType << "\r\n";
    os << "Last-Modified: " << formatHttpDate(stat.lastModified) << "\r\n";

    // optional data
    auto dataSize(source->size());
    os << "Content-Length: " << dataSize << "\r\n";
    if (response.close) { os << "ServerConnection: close\r\n"; }

    os << "\r\n";

    // mark as busy/close
    if (response.close) {
        state_ = State::busyClose;
    }

    // if we have no body or this is reply to HEAD request -> just headers
    if (!dataSize || (request.method == "HEAD")) {
        // just send headers
        auto self(shared_from_this());
        auto headersSent([self, this, request, response]
                         (const bs::error_code &ec
                          , std::size_t bytes)
        {
            if (ec) {
                close(ec);
                return;
            }

            // consume header data
            responseData_.consume(bytes);

            // log what happened
            postLog(self, request, response, bytes);

            // response sent, not busy for now
            makeReady();

            // we are not busy so try next request immediately
            process();
        });

        asio::async_write(socket_, responseData_.data()
                          , strand_.wrap(headersSent));
        return;
    }

    struct Sender : std::enable_shared_from_this<Sender> {
        Sender(const ServerConnection::pointer &conn
               , const Request &request, const Response &response
               , const SinkBase::DataSource::pointer &source
               , std::size_t size)
            : conn(conn), request(request), response(response)
            , source(source), total(), bytesLeft(size), off()
            , buf(1 << 16)
        {}

        void start() {
            auto self(shared_from_this());
            asio::async_write
                (conn->socket_, conn->responseData_.data()
                 , conn->strand_.wrap
                 ([self, this](const bs::error_code &ec
                               , std::size_t bytes)
            {
                headersSent(ec, bytes);
            }));
        }

        void headersSent(const bs::error_code &ec
                         , std::size_t bytes)
        {
            if (ec) {
                conn->close(ec);
                return;
            }

            // consume header data
            conn->responseData_.consume(bytes);
            total += bytes;

            sendBody();
        }

        void sendBody() {
            if (bytesLeft) {
                std::size_t s(0);
                try {
                    s = source->read(buf.data(), buf.size(), off);
                } catch (const std::exception &e) {
                    // force close
                    LOG(err2) << "Error while reading from data source \""
                              << source->name() << "\": <"
                              << e.what() << ">.";
                    source->close();
                    conn->close();
                    return;
                }

                off += s;
                bytesLeft -= s;
                auto self(shared_from_this());
                asio::async_write
                    (conn->socket_, asio::buffer(buf.data(), s)
                     , conn->strand_.wrap
                     ([self, this](const bs::error_code &ec
                                   , std::size_t bytes)
                {
                    bodySent(ec, bytes);
                }));
                return;
            }

            done();
        }

        void bodySent(const bs::error_code &ec, std::size_t bytes)
        {
            if (ec) {
                conn->close(ec);
                return;
            }
            total += bytes;
            sendBody();
        }

        void done() {
            // done with the stream
            source->close();

            // log what happened
            postLog(conn, request, response, total);
            // response sent, not busy for now
            conn->makeReady();
            // we are not busy so try next request immediately
            conn->process();
        }

        ServerConnection::pointer conn;
        Request request;
        Response response;
        SinkBase::DataSource::pointer source;

        std::size_t total;
        std::size_t bytesLeft;
        std::size_t off;
        std::vector<char> buf;
    };

    // run the machine
    std::make_shared<Sender>(shared_from_this(), request, response
                             , source, dataSize)->start();
}

class HttpSink : public ServerSink {
public:
    HttpSink(const Request &request
             , const ServerConnection::pointer &connection)
        : request_(request), connection_(connection)
    {}
    ~HttpSink() {}

private:
    virtual void content_impl(const void *data, std::size_t size
                              , const FileInfo &stat, bool needCopy)
    {
        if (!valid()) { return; }

        Response response;
        response.headers.emplace_back("Content-Type", stat.contentType);
        response.headers.emplace_back
            ("Last-Modified", formatHttpDate(stat.lastModified));

        connection_->sendResponse(request_, response, data, size, !needCopy);
    }

    virtual void content_impl(const SinkBase::DataSource::pointer &source)
    {
        if (!valid()) { return; }

        Response response;
        connection_->sendResponse(request_, response, source);
    }

    virtual void seeOther_impl(const std::string &url)
    {
        if (!valid()) { return; }

        Response response(StatusCode::Found);
        response.headers.emplace_back("Location", url);
        connection_->sendResponse(request_, response);
    }

    virtual void listing_impl(const Listing &list)
    {
        if (!valid()) { return; }

        std::string path;

        std::ostringstream os;
        os << R"RAW(<html>
<head><title>Index of )RAW" << path
           << R"RAW(</title></head>
<body bgcolor="white">
<h1>Index of )RAW"
           << path
           << "\n</h1><hr><pre><a href=\"../\">../</a>\n";

        auto sorted(list);
        std::sort(sorted.begin(), sorted.end());

        for (const auto &item : sorted) {
            switch (item.type) {
            case ListingItem::Type::file:
                os << "<a href=\"" << item.name << "\">"
                   << item.name << "</a>\n";
                break;
            case ListingItem::Type::dir:
                os << "<a href=\"" << item.name << "/\">"
                   << item.name << "/</a>\n";
                break;
            }
        }

        os << R"RAW(</pre><hr></body>
</html>
)RAW";

        content(os.str(), { "text/html; charset=utf-8" });
    }

    virtual void error_impl(const std::exception_ptr &exc)
    {
        if (!valid()) { return; }

        auto sendError([&](StatusCode statusCode
                           , const std::string &body
                           , const std::string &reason)
        {
            LOG(debug)
                << "About to send http error: <" << statusCode << ">.";

            Response response(statusCode);
            response.reason = reason;
            response.headers.emplace_back
                ("Content-Type", "text/html; charset=utf-8");

            connection_->sendResponse
                (request_, response, body.data(), body.size(), true);
        });

        try {
            std::rethrow_exception(exc);
        } catch (const NotFound &e) {
            sendError(StatusCode::NotFound, error404, e.what());
        } catch (const NotAllowed &e) {
            sendError(StatusCode::NotAllowed, error405, e.what());
        } catch (const Unavailable &e) {
            sendError(StatusCode::ServiceUnavailable, error503, e.what());
        } catch (const std::exception &e) {
            sendError(StatusCode::InternalServerError, error500, e.what());
        } catch (...) {
            sendError(StatusCode::InternalServerError, error500, "Uknown");
        }
    }

    virtual bool checkAborted_impl() const {
        return connection_->finished();
    }

    bool valid() const {
        return connection_->valid();
    }

    virtual void setAborter_impl(const AbortedCallback &ac) {
        connection_->setAborter(ac);
    }

    Request request_;
    ServerConnection::pointer connection_;
};

} // namespace detail

void Http::Detail::request(const detail::ServerConnection::pointer &connection
                           , const detail::Request &request)
{
    auto sink(std::make_shared<detail::HttpSink>(request, connection));
    try {
        if ((request.method == "HEAD") || (request.method == "GET")) {
            connection->contentGenerator()->generate(request.uri, sink);
        } else {
            sink->error(utility::makeError<NotAllowed>
                        ("Method %s is not supported.", request.method));
        }
    } catch (...) {
        sink->error();
    }
}

Http::Http(const utility::TcpEndpoint &listen, unsigned int threadCount
           , ContentGenerator &contentGenerator)
    : detail_(std::make_shared<Detail>())
{
    this->listen(listen, contentGenerator);
    detail().start(threadCount);
}

Http::Http()
    : detail_(std::make_shared<Detail>())
{
}

void Http::listen(const utility::TcpEndpoint &listen
                  , const ContentGenerator::pointer &contentGenerator)
{
    detail().listen(listen, contentGenerator);
}

void Http::listen(const utility::TcpEndpoint &listen
                  , ContentGenerator &contentGenerator)
{
    detail().listen(listen, ContentGenerator::pointer
                    (&contentGenerator, [](void*){}));
}

void Http::start(unsigned int threadCount)
{
    detail().start(threadCount);
}

void Http::stop()
{
    detail().stop();
}

void Http::Detail::fetch(const utility::Uri &location
                         , const ClientSink::pointer &sink)
{
    LOG(info4) << "About to fetch <" << join(location) << ">";

    // resolve hostname
    resolver_.async_resolve(tcp::resolver::query
                            (location.host, location.schema)
                            , [this, location, sink](const bs::error_code &ec
                             , tcp::resolver::iterator i)
    {
        if (ec) {
            sink->error(bs::system_error(ec, "DNS resolution failed"));
            return;
        }

        for (tcp::resolver::iterator e; i != e; ++i) {
            LOG(info4) << "Resolved <" << i->host_name() << ">: <"
                       << i->endpoint().address().to_string() << ":"
                       << i->endpoint().port() << ">.";
        }
    });
}

void Http::fetch_impl(const utility::Uri &location
                      , const ClientSink::pointer &sink)
{
    detail().fetch(location, sink);
}

} // namespace http
