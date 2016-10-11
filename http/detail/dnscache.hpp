#ifndef http_detail_dnscache_hpp_included_
#define http_detail_dnscache_hpp_included_

#include <memory>
#include <string>
#include <vector>
#include <mutex>

#include <boost/asio.hpp>
#include <boost/format.hpp>

#include "dbglog/dbglog.hpp"
#include "utility/uri.hpp"

namespace http { namespace detail {

class DnsCache {
public:
    DnsCache(boost::asio::io_service &ios)
        : ios_(ios), resolver_(ios)
    {}

    template <typename ResolveHandler>
    void resolve(const utility::Uri &uri, const ResolveHandler &rh);

    typedef std::vector<boost::asio::ip::tcp::endpoint> Endpoints;

private:
    boost::asio::io_service &ios_;
    boost::asio::ip::tcp::resolver resolver_;

    std::mutex mutex_;

    struct Entry {
        Endpoints endpoints;
        std::time_t expires;

        typedef std::map<std::string, Entry> map;
    };

    Entry::map cache_;
};

template <typename ResolveHandler>
void DnsCache::resolve(const utility::Uri &uri, const ResolveHandler &rh)
{
    const auto key((uri.port() <= 0)
                   ? uri.host()
                   : str(boost::format("%s:%d") % uri.host() % uri.port()));

    Endpoints endpoints;
    {
        std::unique_lock<std::mutex> lock(mutex_);

        auto fcache(cache_.find(key));
        if (fcache != cache_.end()) {
            // something cached
            if (fcache->second.expires >= std::time(nullptr)) {
                // still valid
                endpoints = fcache->second.endpoints;
            }
        }
    }

    // call callback outside of any lock
    if (!endpoints.empty()) {
        ios_.post([=]() {
                rh(boost::system::error_code(), endpoints);
            });
        return;
    }

    // not cached
    // resolve hostname
    resolver_.async_resolve
        (boost::asio::ip::tcp::resolver::query
         (uri.host(), uri.scheme())
         , [rh, key, this](const boost::system::error_code &ec
                           , boost::asio::ip::tcp::resolver::iterator i)
          mutable
    {
        if (ec) {
            rh(ec, Endpoints());
            return;
        }

        Entry entry;
        entry.expires = std::time(nullptr) + 300;
        for (decltype(i) e; i != e; ++i) {
            entry.endpoints.push_back(i->endpoint());
        }

        // cache
        {
            std::unique_lock<std::mutex> lock(mutex_);

            // insert/replace
            auto res(cache_.insert(Entry::map::value_type(key, entry)));
            if (!res.second) {
                res.first->second = entry;
            }
        }

        // and callback
        ios_.post([=]() {
                rh(ec, entry.endpoints);
            });
        return;
    });
}

} } // namespace http::detail

#endif // http_detail_dnscache_hpp_included_
