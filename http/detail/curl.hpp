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

#include "../constants.hpp"
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
                     , const std::string &location
                     , const ClientSink::pointer &sink
                     , const ContentFetcher::RequestOptions &options);
    ~ClientConnection();

    ::CURL* handle() { return easy_; };

    void notify(::CURLcode result);

    void store(const char *data, std::size_t size);

    void header(const char *data, std::size_t size);

private:
    void processHeader();

    ::CURL *easy_;
    ::curl_slist *headers_;
    CurlClient &owner_;
    std::string location_;
    ClientSink::pointer sink_;

    std::string headerName_;
    std::string headerValue_;

    std::time_t maxAge_;
    std::time_t expires_;
    std::string content_;
};

typedef ip::tcp::socket Socket;
typedef std::shared_ptr<Socket> SocketPointer;
typedef std::map<curl_socket_t, SocketPointer> SocketMap;

class CurlClient : boost::noncopyable {
public:
    typedef std::shared_ptr<CurlClient> pointer;
    typedef std::vector<CurlClient::pointer> list;
    CurlClient(int id);
    ~CurlClient();

    void fetch(const std::string &location
               , const ClientSink::pointer &sink
               , const ContentFetcher::RequestOptions &options);

    void add(ClientConnection &conn);
    void remove(ClientConnection &conn);

    int handle(::CURL *easy, ::curl_socket_t s, int what
               , void *socketp);

    int timeout(long int timeout);

    ::curl_socket_t open(::curlsocktype purpose,
                         ::curl_sockaddr *address);

    int close(::curl_socket_t s);

private:
    void start(unsigned int id);
    void stop();
    void run(unsigned int id);

    void action(Socket *socket = nullptr, int what = 0);

    ::CURLM *multi_;
    asio::io_service ios_;
    boost::optional<asio::io_service::work> work_;
    std::thread worker_;
    asio::deadline_timer timer_;

    struct HandleIdx {};

    ClientConnection::set connections_;
    SocketMap sockets_;
    int runningTransfers_;
};

} } // namespace http::detail

#endif // http_detail_curl_hpp_included_
