#ifndef http_detail_acceptor_hpp_included_
#define http_detail_acceptor_hpp_included_

#include <memory>
#include <string>
#include <vector>

#include "utility/enum-io.hpp"

#include "./detail.hpp"

namespace http { namespace detail {

class Acceptor {
public:
    typedef std::shared_ptr<Acceptor> pointer;
    typedef std::vector<pointer> list;

    Acceptor(Http::Detail &owner, asio::io_service &ios
             , const utility::TcpEndpoint &listen
             , const ContentGenerator::pointer &contentGenerator)
        : owner_(owner), ios_(ios), acceptor_(ios_, listen.value, true)
        , contentGenerator_(contentGenerator)
    {
        start();
    }

    void start();

    utility::TcpEndpoint localEndpoint() const {
        return utility::TcpEndpoint(acceptor_.local_endpoint());
    }

private:
    Http::Detail &owner_;
    asio::io_service &ios_;
    tcp::acceptor acceptor_;
    ContentGenerator::pointer contentGenerator_;
};

} } // namespace http::detail

#endif // http_detail_acceptor_hpp_included_
