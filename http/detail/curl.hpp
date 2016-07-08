#ifndef http_detail_curl_hpp_included_
#define http_detail_curl_hpp_included_

#include <memory>
#include <set>
#include <vector>
#include <string>
#include <atomic>
#include <thread>

#include <boost/noncopyable.hpp>
#include <boost/optional.hpp>
#include <boost/asio.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/mem_fun.hpp>

#include <curl/curl.h>

#include "utility/uri.hpp"

#include "../sink.hpp"
#include "../contentfetcher.hpp"

namespace http { namespace detail {

namespace asio = boost::asio;
namespace ip = asio::ip;
namespace bs = boost::system;
namespace bmi = boost::multi_index;
typedef asio::ip::tcp tcp;

class CurlClient;

class ClientConnection
    : boost::noncopyable
    , public std::enable_shared_from_this<ClientConnection>
{
public:
    typedef std::set<ClientConnection*> set;

    ClientConnection(CurlClient &owner
                     , const utility::Uri &location
                     , const ClientSink::pointer &sink
                     , const ContentFetcher::RequestOptions &options);
    ~ClientConnection();

    ::CURL* handle() { return easy_; };

    void notify(::CURLcode result);

    void store(char *data, std::size_t size);

private:
    ::CURL *easy_;
    CurlClient &owner_;
    utility::Uri location_;
    std::string url_;
    ClientSink::pointer sink_;

    std::string content_;
};

struct CurlSocket
    : public std::enable_shared_from_this<CurlSocket>
    , boost::noncopyable
{
    typedef std::set<CurlSocket*> set;

    CurlSocket(ClientConnection *conn, asio::io_service &ios
               , curl_socket_t native)
        : conn(conn), socket(ios)
    {
        // TODO: find out protocol
        socket.assign(tcp::v4(), native);
    }

    ClientConnection *conn;
    tcp::socket socket;
};

class CurlClient : boost::noncopyable {
public:
    typedef std::shared_ptr<CurlClient> pointer;
    typedef std::vector<CurlClient::pointer> list;
    CurlClient(int id);
    ~CurlClient();

    void fetch(const utility::Uri &location
               , const ClientSink::pointer &sink
               , const ContentFetcher::RequestOptions &options);

    void add(ClientConnection &conn);
    void remove(ClientConnection &conn);

    int handle(::CURL *easy, ::curl_socket_t s, int what
               , void *socketp);

    int timeout(long int timeout);

private:
    void start(unsigned int id);
    void stop();
    void run(unsigned int id);

    void action(CurlSocket *socket = nullptr, int what = 0);

    ::CURLM *multi_;
    asio::io_service ios_;
    boost::optional<asio::io_service::work> work_;
    std::thread worker_;
    asio::deadline_timer timer_;

    struct HandleIdx {};

    ClientConnection::set connections_;
    CurlSocket::set sockets_;
    int runningTransfers_;
};

} } // namespace http::detail

#endif // http_detail_curl_hpp_included_
