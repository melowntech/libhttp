#ifndef http_detail_detail_hpp_included_
#define http_detail_detail_hpp_included_

#include <memory>
#include <set>
#include <string>
#include <atomic>
#include <thread>

#include <boost/noncopyable.hpp>
#include <boost/asio.hpp>
#include <boost/optional.hpp>

#include "utility/buildsys.hpp"

#include "../http.hpp"
#include "../contentfetcher.hpp"
#include "./dnscache.hpp"
#include "./curl.hpp"

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

class Http::Detail
    : boost::noncopyable
    , public ContentFetcher
{
public:
    Detail();

    ~Detail() { stop(); }

    void request(const std::shared_ptr<detail::ServerConnection> &connection
                 , const detail::Request &request);

    void addServerConnection
    (const std::shared_ptr<detail::ServerConnection> &conn);
    void removeServerConnection
    (const std::shared_ptr<detail::ServerConnection> &conn);

    /** Starts server threads.
     */
    void startServer(std::size_t count);

    /** Starts server threads.
     */
    void startClient(std::size_t count);

    /** Strop all threads.
     */
    void stop();

    utility::TcpEndpoint
    listen(const utility::TcpEndpoint &listen
           , const ContentGenerator::pointer &contentGenerator);

    static Detail& detail(const Http &http) { return *http.detail_; }

    asio::io_service& ioService() { return ios_; }

    void serverHeader(const std::string &value) {
        serverHeader_ = value;
    }

    const std::string& serverHeader() const { return serverHeader_; }

private:
    virtual void fetch_impl(const std::string &location
                            , const ClientSink::pointer &sink
                            , const RequestOptions &options);

    void worker(std::size_t id);

    asio::io_service ios_;
    boost::optional<asio::io_service::work> work_;
    detail::DnsCache dnsCache_;
    std::vector<std::thread> workers_;

    std::vector<std::shared_ptr<detail::Acceptor>> acceptors_;
    std::set<std::shared_ptr<detail::ServerConnection>> connections_;
    std::mutex connMutex_;
    std::condition_variable connCond_;
    std::atomic<bool> running_;
    std::string serverHeader_;

    std::mutex clientMutex_;

    /** CURL based clients.
     */
    detail::CurlClient::list clients_;

    /** Current client. Workload is distributed in round-robin manner.
     */
    detail::CurlClient::list::iterator currentClient_;
};

} // namespace http

#endif // http_detail_detail_hpp_included_
