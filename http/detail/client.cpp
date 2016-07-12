#include <boost/format.hpp>

#include "dbglog/dbglog.hpp"

#include "utility/raise.hpp"

#include "../error.hpp"

#include "./detail.hpp"

#include "./curl.hpp"

namespace http { namespace detail {

#define CHECK_CURL_STATUS(op, what)                                     \
    do {                                                                \
        auto res(op);                                                   \
        if (res != CURLE_OK) {                                          \
            LOGTHROW(err2, Error)                                       \
                << "Failed to perform easy CURL operation <" << what    \
                << ">:" << res << ", "                                  \
                << ::curl_easy_strerror(res)                            \
                << ">.";                                                \
        }                                                               \
    } while (0)

#define LOG_CURL_STATUS(op, what)                                       \
    do {                                                                \
        auto res(op);                                                   \
        if (res != CURLE_OK) {                                          \
            LOG(err2)                                                   \
                << "Failed to perform easy CURL operation <" << what    \
                << ">:" << res << ", "                                  \
                << ::curl_easy_strerror(res)                            \
                << ">.";                                                \
        }                                                               \
    } while (0)

#define SETOPT(name, value)                                     \
    CHECK_CURL_STATUS(::curl_easy_setopt(easy_, name, value)    \
                      , "curl_easy_setopt")

#define CHECK_CURLM_STATUS(op, what)                                    \
    do {                                                                \
        auto res(op);                                                   \
        if (res != CURLM_OK) {                                          \
            LOGTHROW(err2, Error)                                       \
                << "Failed to perform multi CURL operation <" << what   \
                << ">:" << res << ", "                                  \
                << ::curl_multi_strerror(res)                           \
                << ">.";                                                \
        }                                                               \
    } while (0)

#define LOG_CURLM_STATUS(op, what)                                      \
    do {                                                                \
        auto res(op);                                                   \
        if (res != CURLM_OK) {                                          \
            LOG(err2)                                                   \
                << "Failed to perform multi CURL operation <" << what   \
                << ">:" << res << ", "                                  \
                << ::curl_multi_strerror(res)                           \
                << ">.";                                                \
        }                                                               \
    } while (0)

#define MSETOPT(name, value)                                       \
    CHECK_CURLM_STATUS(::curl_multi_setopt(multi_, name, value)    \
                       , "curl_multi_setopt")

ClientConnection* connFromEasy(CURL *easy)
{
    ClientConnection *conn(nullptr);
    LOG_CURL_STATUS(::curl_easy_getinfo(easy, CURLINFO_PRIVATE, &conn)
                    , "curl_easy_getinfo");
    return conn;
}

extern "C" {

std::size_t http_clientconnection_write(void *ptr, std::size_t size
                                        , std::size_t nmemb, void *userp)
{
    auto count(size * nmemb);
    static_cast<ClientConnection*>(userp)
        ->store(static_cast<char*>(ptr), count);
    return count;
}

::curl_socket_t http_curlclient_opensocket(void *clientp,
                                           ::curlsocktype purpose,
                                           ::curl_sockaddr *address)
{
    return static_cast<CurlClient*>(clientp)->open(purpose, address);
}

int http_curlclient_closesocket(void *clientp, ::curl_socket_t item)
{
    return static_cast<CurlClient*>(clientp)->close(item);
}

} // extern "C"

ClientConnection
::ClientConnection(CurlClient &owner
                   , const std::string &location
                   , const ClientSink::pointer &sink
                   , const ContentFetcher::RequestOptions &options)
    : easy_(::curl_easy_init()), owner_(owner)
    , location_(location), sink_(sink)
{
    LOG(info2) << "Starting transfer from <" << location_ << ">.";

    if (!easy_) {
        LOGTHROW(err2, Error)
            << "Failed to create easy CURL handle.";
    }

    struct Guard {
        ::CURL *easy;
        ~Guard() { if (easy) {
                LOG(warn2) << "Destroying easy handle due to an error.";
                ::curl_easy_cleanup(easy);
            } }
    } guard{ easy_ };

    // set myself as a private data
    SETOPT(CURLOPT_PRIVATE, this);

    // switch off SIGALARM
    SETOPT(CURLOPT_NOSIGNAL, 1L);
    // retain last modified time
    SETOPT(CURLOPT_FILETIME, 1L);
    // force HTTP/1.1
    SETOPT(CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);

    // use user agent
    if (!options.userAgent.empty()) {
        SETOPT(CURLOPT_USERAGENT, options.userAgent.c_str());
    }

    // write op
    SETOPT(CURLOPT_WRITEFUNCTION, &http_clientconnection_write);
    SETOPT(CURLOPT_WRITEDATA, this);

    // follow redirects
    SETOPT(CURLOPT_FOLLOWLOCATION, long(options.followRedirects));

    // and finally set url
    SETOPT(CURLOPT_URL, location_.c_str());

    SETOPT(CURLOPT_OPENSOCKETFUNCTION, &http_curlclient_opensocket);
    SETOPT(CURLOPT_OPENSOCKETDATA, &owner);
    SETOPT(CURLOPT_CLOSESOCKETFUNCTION, &http_curlclient_closesocket);
    SETOPT(CURLOPT_CLOSESOCKETDATA, &owner);

    // add to owner
    owner_.add(*this);

    // release from guard
    guard.easy = nullptr;
}

ClientConnection::~ClientConnection()
{
    if (!easy_) { return; }
    owner_.remove(*this);
    ::curl_easy_cleanup(easy_);
}

void ClientConnection::notify(::CURLcode result)
{
    LOG(info2) << "Transfer from <" << location_ << "> finished.";

    if (result != CURLE_OK) {
        sink_->error(utility::makeError<Error>
                     ("Transfer of <%s> failed: <%d, %s>."
                      , location_, result, ::curl_easy_strerror(result)));
        return;
    }

    try {
        long int httpCode(500);
        CHECK_CURL_STATUS(::curl_easy_getinfo
                          (easy_, CURLINFO_RESPONSE_CODE, &httpCode)
                          , "curl_easy_getinfo");
        switch (httpCode / 100) {
        case 2: {
            // cool, we have some content, fetch info and report it
            long int lastModified;
            CHECK_CURL_STATUS(::curl_easy_getinfo
                              (easy_, CURLINFO_FILETIME, &lastModified)
                              , "curl_easy_getinfo");
            char *contentType;
            CHECK_CURL_STATUS(::curl_easy_getinfo
                              (easy_, CURLINFO_CONTENT_TYPE, &contentType)
                              , "curl_easy_getinfo");

            // TODO: fetch cache control header using CURLOPT_HEADERFUNCTION

            sink_->content(content_
                           , http::SinkBase::FileInfo(contentType
                                                      , lastModified));
            break;
        }

        case 3: {
            char *url;
            LOG_CURL_STATUS(::curl_easy_getinfo
                            (easy_, CURLINFO_EFFECTIVE_URL, &url)
                            , "curl_easy_getinfo");
            sink_->seeOther(url);
            break;
        }

        case 4:
            switch (httpCode) {
            case 405:
                sink_->error(utility::makeError<NotAllowed>
                             ("Method Not Allowed"));
                break;

            case 404:
                sink_->error(utility::makeError<NotFound>
                             ("Not Found"));
                break;
            }
            break;

        default:
            switch (httpCode) {
            case 503:
                sink_->error(utility::makeError<Unavailable>
                             ("Service Not Available"));
                break;

            default:
                sink_->error(utility::makeError<InternalError>
                             ("Server error %d.", httpCode));
                break;
            }
            break;
        }
    } catch (...) {
        sink_->error();
    }
}

void ClientConnection::store(char *data, std::size_t size)
{
    content_.append(data, size);
}

extern "C" {

int http_curlclient_socket(::CURL *easy, ::curl_socket_t s, int what
                           , void *userp, void *socketp)
{
    return static_cast<CurlClient*>(userp)
        ->handle(easy, s, what, socketp);
}

int http_curlclient_timer(CURLM*, long int timeout, void *userp)
{
    return static_cast<CurlClient*>(userp)->timeout(timeout);
}

} // extern "C"

CurlClient::CurlClient(int id)
    : multi_(::curl_multi_init())
    , work_(std::ref(ios_))
    , timer_(ios_)
    , runningTransfers_()
{
    if (!multi_) {
        LOGTHROW(err2, Error)
            << "Failed to create mutli CURL handle.";
    }

    MSETOPT(CURLMOPT_SOCKETFUNCTION, &http_curlclient_socket);
    MSETOPT(CURLMOPT_SOCKETDATA, this);
    MSETOPT(CURLMOPT_TIMERFUNCTION, &http_curlclient_timer);
    MSETOPT(CURLMOPT_TIMERDATA, this);

    start(id);
}

CurlClient::~CurlClient()
{
    if (!multi_) { return; }

    // stop the machinery: this gives us free hand to manipulate with
    // connections
    stop();

    // destroy sockets and connection
    for (auto conn : connections_) { delete conn; }
    connections_.clear();
    sockets_.clear();

    LOG_CURLM_STATUS(::curl_multi_cleanup(multi_)
                     , "curl_multi_cleanup");
}

void CurlClient::start(unsigned int id)
{
    std::thread worker(&CurlClient::run, this, id);
    worker_.swap(worker);
}

void CurlClient::stop()
{
    work_ = boost::none;
    worker_.join();
    ios_.stop();
}

void CurlClient::run(unsigned int id)
{
    dbglog::thread_id(str(boost::format("chttp:%u") % id));
    LOG(info2) << "Spawned HTTP client worker id:" << id << ".";

    for (;;) {
        try {
            ios_.run();
            LOG(info2) << "Terminated HTTP client worker id:" << id << ".";
            return;
        } catch (const std::exception &e) {
            LOG(err3)
                << "Uncaught exception in HTTP client worker: <" << e.what()
                << ">. Going on.";
        }
    }
}

void CurlClient::add(ClientConnection &conn)
{
    LOG(info1) << "Adding connection " << conn.handle();
    CHECK_CURLM_STATUS(::curl_multi_add_handle
                       (multi_, conn.handle())
                       , "curl_multi_add_handle");
}

void CurlClient::remove(ClientConnection &conn)
{
    LOG(info1) << "Removing connection " << conn.handle();
    CHECK_CURLM_STATUS(::curl_multi_remove_handle
                       (multi_, conn.handle())
                       , "curl_multi_remove_handle");
}

void CurlClient::fetch(const std::string &location
                       , const ClientSink::pointer &sink
                       , const ContentFetcher::RequestOptions &options)
{
    ios_.post([=]()
    {
        try {
            std::unique_ptr<ClientConnection> conn
                (new ClientConnection(*this, location, sink, options));
            connections_.insert(conn.release());
        } catch (...) {
            sink->error();
        }
    });
}

::curl_socket_t CurlClient::open(::curlsocktype purpose,
                                 ::curl_sockaddr *address)
{
    if (purpose == CURLSOCKTYPE_IPCXN && address->family == AF_INET)
    {
        // remember socket
        auto socket(std::make_shared<Socket>(ios_));

        bs::error_code ec;
        socket->open(ip::tcp::v4(), ec);

        if (ec) {
            LOG(warn2) << "Failed to open TCP socket: <" << ec << ">.";
            return CURL_SOCKET_BAD;
        }

        auto native(socket->native_handle());
        sockets_.insert(SocketMap::value_type(native, socket));
        return native;
    }

    // unsupported
    return CURL_SOCKET_BAD;
}

int CurlClient::close(::curl_socket_t s)
{
    sockets_.erase(s);
    return 0;
}

int CurlClient::handle(::CURL*, ::curl_socket_t s, int what
                       , void *socketp)
{
    if (what == CURL_POLL_REMOVE) {
        // nothing to do
        return 0;
    }

    Socket *socket;
    if (!socketp) {
        // find
        auto fsockets(sockets_.find(s));
        if (fsockets == sockets_.end()) {
            // OOPS, not found
            LOG(warn2) << "Unknown socket " << s << " in handle callback.";
            return -1;
        }

        // assign
        socket = fsockets->second.get();

        // assign connection to socket
        CHECK_CURLM_STATUS
            (::curl_multi_assign(multi_, s, socket)
            , "curl_multi_assign");
    } else {
        socket = static_cast<Socket*>(socketp);
    }

    if (what & CURL_POLL_IN) {
        socket->async_read_some(asio::null_buffers()
                                , [=] (const bs::error_code &ec
                                       , std::size_t)
        {
            if (!ec) { action(socket, what); }
        });
    }

    if (what & CURL_POLL_OUT) {
        socket->async_write_some(asio::null_buffers()
                                 , [=] (const bs::error_code &ec
                                        , std::size_t)
        {
            if (!ec) { action(socket, what); }
        });
    }

    return 0;
}

int CurlClient::timeout(long int timeout)
{
    timer_.cancel();

    if (timeout > 0) {
        timer_.expires_from_now(boost::posix_time::millisec(timeout));
        timer_.async_wait([this](const bs::error_code &ec)
        {
            if (!ec) { action(); }
        });
    } else {
        action();
    }
    return 0;
}

void CurlClient::action(Socket *socket, int what)
{
    CHECK_CURLM_STATUS(::curl_multi_socket_action
                       (multi_
                        , (socket ? socket->native_handle()
                           : CURL_SOCKET_TIMEOUT)
                        , what, &runningTransfers_)
                       , "curl_multi_socket_action");

    // check message info
    int left(0);
    while (auto *msg = ::curl_multi_info_read(multi_, &left)) {
        if (msg->msg != CURLMSG_DONE) { continue; }

        auto *conn(connFromEasy(msg->easy_handle));
        if (!conn) {
            LOG(info2) << "No connection associated with CURL easy handle "
                       << msg->easy_handle << "; skipping.";
            continue;
        }

        // notify finished transfer
        conn->notify(msg->data.result);

        // get rid of connection
        connections_.erase(conn);
        delete conn;
    }
}

} } // namespace http::detail

namespace http {

void Http::Detail::fetch_impl(const std::string &location
                              , const ClientSink::pointer &sink
                              , const ContentFetcher::RequestOptions &options)
{
    if (clients_.empty()) {
        LOGTHROW(err2, Error)
            << "Cannot perform fetch request: no client is running.";
    }

    (*currentClient_++)->fetch(location, sink, options);
    if (currentClient_ == clients_.end()) {
        currentClient_ = clients_.begin();
    }
}

} // namespace http
