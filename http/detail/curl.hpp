/**
 * Copyright (c) 2017 Melown Technologies SE
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * *  Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
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
    std::string location_;
    ClientSink::pointer sink_;

    std::string headerName_;
    std::string headerValue_;

    std::time_t maxAge_;
    std::time_t expires_;
    std::string content_;
};

struct Socket : boost::noncopyable {
    typedef std::shared_ptr<Socket> pointer;
    typedef std::map<curl_socket_t, pointer> map;

    ip::tcp::socket socket;
    bool read;
    bool write;
    bool readInProgress;
    bool writeInProgress;

    Socket(asio::io_service &ios)
        : socket(ios), read(false), write(false)
        , readInProgress(false), writeInProgress(false)
        , finished_(false)
    {}

    bool inProgress() const { return readInProgress || writeInProgress; }

    void finish() {
        read = write = readInProgress = writeInProgress = false;
        finished_ = true;
    }

    bool finished() const {
        return finished_ && !inProgress();
    }

    auto handle() -> decltype(socket.native_handle()) {
        return socket.native_handle();
    }

private:
    bool finished_;
};

class CurlClient : boost::noncopyable {
public:
    typedef std::shared_ptr<CurlClient> pointer;
    typedef std::vector<CurlClient::pointer> list;
	CurlClient(int id, const ContentFetcher::Options *options = nullptr);
    ~CurlClient();

    void fetch(const std::string &location
               , const ClientSink::pointer &sink
               , const ContentFetcher::RequestOptions &options);

    void add(std::unique_ptr<ClientConnection> &&conn);
    void remove(ClientConnection *conn);

    int handle_cb(::CURL *easy, ::curl_socket_t s, int what
                  , void *socketp);

    int timeout_cb(long int timeout);

    ::curl_socket_t open_cb(::curlsocktype purpose,
                            ::curl_sockaddr *address);

    int close_cb(::curl_socket_t s);

private:
    void start(unsigned int id);
    void stop();
    void run(unsigned int id);

    void action(Socket *socket = nullptr, int what = 0);

    void prepareRead(Socket *socket);
    void prepareWrite(Socket *socket);
    void shred(Socket *socket);

    ::CURLM *multi_;
    asio::io_service ios_;
    boost::optional<asio::io_service::work> work_;
    std::thread worker_;
    asio::deadline_timer timer_;

    struct HandleIdx {};

    ClientConnection::set connections_;
    Socket::map sockets_;
    int runningTransfers_;
};

} } // namespace http::detail

#endif // http_detail_curl_hpp_included_
