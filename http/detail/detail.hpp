#ifndef http_detail_detail_hpp_included_
#define http_detail_detail_hpp_included_

#include <memory>
#include <string>
#include <atomic>
#include <thread>

#include <boost/noncopyable.hpp>
#include <boost/asio.hpp>

#include "utility/uri.hpp"

#include "../http.hpp"

namespace http {

namespace asio = boost::asio;
namespace ip = asio::ip;
namespace bs = boost::system;
namespace ubs = utility::buildsys;
typedef asio::ip::tcp tcp;

namespace detail {
class ServerConnection;

class Request;
class Acceptor;
} // namespace detail

class Http::Detail : boost::noncopyable {
public:
    Detail();

    ~Detail() { stop(); }

    void request(const std::shared_ptr<detail::ServerConnection> &connection
                 , const detail::Request &request);

    void addServerConnection
    (const std::shared_ptr<detail::ServerConnection> &conn);
    void removeServerConnection
    (const std::shared_ptr<detail::ServerConnection> &conn);

    void start(std::size_t count);
    void stop();
    void listen(const utility::TcpEndpoint &listen
                , const ContentGenerator::pointer &contentGenerator);

    void fetch(const utility::Uri &location
               , const ClientSink::pointer &sink);

private:
    void worker(std::size_t id);

    asio::io_service ios_;
    boost::optional<asio::io_service::work> work_;
    tcp::resolver resolver_;
    std::vector<std::thread> workers_;

    std::vector<std::shared_ptr<detail::Acceptor>> acceptors_;
    std::set<std::shared_ptr<detail::ServerConnection>> connections_;
    std::mutex connMutex_;
    std::condition_variable connCond_;
    std::atomic<bool> running_;
};

} // namespace http

#endif // http_detail_detail_hpp_included_
