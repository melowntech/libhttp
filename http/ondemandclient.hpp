#ifndef http_ondemandclient_hpp_included_
#define http_ondemandclient_hpp_included_

#include <memory>
#include <string>
#include <mutex>

#include "./resourcefetcher.hpp"

namespace http {

/** On-demand (dormant) HTTP client. IO threads are started on first use.
 */
class OnDemandClient {
public:
    /** Creates dormant HTTP client.
     *
     *  \param threads number of IO threads
     */
    OnDemandClient(std::size_t threads = 1);

    utility::ResourceFetcher& fetcher() const;

    struct Detail;
    friend struct Detail;

private:
    mutable std::shared_ptr<Detail> detail_;
};

} // namespace http

#endif // http_ondemandclient_hpp_included_
