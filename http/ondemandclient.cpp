#include <boost/optional.hpp>

#include "./ondemandclient.hpp"
#include "./contentfetcher.hpp"
#include "./detail/curl.hpp"

namespace http {

struct OnDemandClient::Detail : public ContentFetcher
{
    Detail(std::size_t threadCount)
        : threadCount(threadCount ? threadCount : 1)
        , fetcher(*this) {}

    std::mutex mutex_;
    std::size_t threadCount;
    detail::CurlClient::list clients;
    detail::CurlClient::list::iterator currentClient;
    ResourceFetcher fetcher;

    virtual void fetch_impl(const std::string &location
                            , const ClientSink::pointer &sink
                            , const ContentFetcher::RequestOptions &options);
};

void OnDemandClient::Detail
::fetch_impl(const std::string &location
             , const ClientSink::pointer &sink
             , const ContentFetcher::RequestOptions &options)
{
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        if (clients.empty()) {
            for (std::size_t i(0); i < threadCount; ++i) {
                clients.push_back(std::make_shared<detail::CurlClient>(i));
            }
            currentClient = clients.begin();
        }

        (*currentClient)->fetch(location, sink, options);
        if (++currentClient == clients.end()) {
            currentClient = clients.begin();
        }
    } catch (...) {
        sink->error();
    }
}

OnDemandClient::OnDemandClient(std::size_t threads)
    : detail_(std::make_shared<Detail>(threads))
{}

utility::ResourceFetcher& OnDemandClient::fetcher() const
{
    return detail_->fetcher;
}

} // namespace http
