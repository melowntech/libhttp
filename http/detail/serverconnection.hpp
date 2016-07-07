#ifndef http_detail_serverconnection_hpp_included_
#define http_detail_serverconnection_hpp_included_

#include <memory>
#include <string>
#include <vector>

#include "utility/enum-io.hpp"

#include "./detail.hpp"

namespace http { namespace detail {

class ServerConnection
    : boost::noncopyable
    , public std::enable_shared_from_this<ServerConnection>
{
public:
    typedef std::shared_ptr<ServerConnection> pointer;
    typedef std::set<pointer> set;

    ServerConnection(Http::Detail &owner, asio::io_service &ios
               , const ContentGenerator::pointer &contentGenerator)
        : id_(++idGenerator_)
        , lm_(dbglog::make_module(str(boost::format("conn:%s") % id_)))
        , owner_(owner), ios_(ios), strand_(ios), socket_(ios_)
        , requestData_(1024) // max line size
        , state_(State::ready)
        , contentGenerator_(contentGenerator)
    {}

    tcp::socket& socket() {return  socket_; }

    void sendResponse(const Request &request, const Response &response
                      , const std::string &data, bool persistent = false)
    {
        sendResponse(request, response, data.data(), data.size(), persistent);
    }

    void sendResponse(const Request &request, const Response &response
                      , const void *data = nullptr, const size_t size = 0
                      , bool persistent = false);

    void sendResponse(const Request &request, const Response &response
                      , const SinkBase::DataSource::pointer &source);

    void start();

    bool valid() const;

    void close();

    dbglog::module& lm() { return lm_; }

    bool finished() const;

    void setAborter(const ServerSink::AbortedCallback &ac);

    ContentGenerator::pointer contentGenerator() { return contentGenerator_; }

private:
    void startRequest();
    void readRequest();
    void readHeader();

    Request pop() {
        auto r(requests_.front());
        requests_.erase(requests_.begin());
        return r;
    }

    void process();
    void badRequest();
    void close(const bs::error_code &ec);

    void makeReady();

    void aborted();

    static std::atomic<std::size_t> idGenerator_;

    std::atomic<std::size_t> id_;
    dbglog::module lm_;

    Http::Detail &owner_;

    asio::io_service &ios_;
    asio::strand strand_;
    tcp::socket socket_;
    asio::streambuf requestData_;
    asio::streambuf responseData_;

    Request::list requests_;

    enum class State { ready, busy, busyClose, closed };
    State state_;

    std::mutex acMutex_;
    ServerSink::AbortedCallback ac_;
    ContentGenerator::pointer contentGenerator_;
};

} } // namespace http::detail

#endif // http_detail_serverconnection_hpp_included_
