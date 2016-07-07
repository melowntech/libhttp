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

    void fetch(const utility::Uri &location
               , const ClientSink::pointer &sink);

    void fetch(const std::string &location
               , const ClientSink::pointer &sink);

private:
    virtual void fetch_impl(const utility::Uri &location
                            , const ClientSink::pointer &sink) = 0;
};

// inlines

inline void ContentFetcher::fetch(const utility::Uri &location
                                  , const ClientSink::pointer &sink)
{
    return fetch_impl(location, sink);
}

inline void ContentFetcher::fetch(const std::string &location
                                  , const ClientSink::pointer &sink)
{
    return fetch_impl(location, sink);
}

} // namespace http

#endif // http_contentfetcher_hpp_included_

