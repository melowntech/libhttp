#ifndef http_contentfetcher_hpp_included_
#define http_contentfetcher_hpp_included_

#include <ctime>
#include <string>
#include <vector>
#include <memory>
#include <exception>

#include "utility/uri.hpp"

#include "./sink.hpp"

namespace http {

class ContentFetcher {
public:
    typedef std::shared_ptr<ContentFetcher> pointer;

    virtual ~ContentFetcher() {}

    struct RequestOptions {
        RequestOptions() : followRedirects(true) {}

        bool followRedirects;
        std::string userAgent;
    };

    void fetch(const utility::Uri &location
               , const ClientSink::pointer &sink
               , const RequestOptions &options = RequestOptions());

    void fetch(const std::string &location
               , const ClientSink::pointer &sink
               , const RequestOptions &options = RequestOptions());

private:
    virtual void fetch_impl(const utility::Uri &location
                            , const ClientSink::pointer &sink
                            , const RequestOptions &options) = 0;
};

// inlines

inline void ContentFetcher::fetch(const utility::Uri &location
                                  , const ClientSink::pointer &sink
                                  , const RequestOptions &options)
{
    return fetch_impl(location, sink, options);
}

inline void ContentFetcher::fetch(const std::string &location
                                  , const ClientSink::pointer &sink
                                  , const RequestOptions &options)
{
    return fetch_impl(location, sink, options);
}

} // namespace http

#endif // http_contentfetcher_hpp_included_

