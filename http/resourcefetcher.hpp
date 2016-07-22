#ifndef http_resourcefetcher_hpp_included_
#define http_resourcefetcher_hpp_included_

#include <boost/asio.hpp>

#include "utility/resourcefetcher.hpp"

namespace http {

class ContentFetcher;

/** Fetches HTTP resources from URLs.
 */
class ResourceFetcher : public utility::ResourceFetcher {
public:
    ResourceFetcher(ContentFetcher &contentFetcher
                    , boost::asio::io_service *ios = nullptr)
        : contentFetcher_(contentFetcher), queryIos_(ios)
    {}

private:
    virtual void perform_impl(MultiQuery query, const Done &done) const;

    ContentFetcher &contentFetcher_;
    boost::asio::io_service *queryIos_;
};

} // namespace http

#endif // http_resourcefetcher_hpp_included_
