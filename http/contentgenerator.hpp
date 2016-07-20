#ifndef http_contentgenerator_hpp_included_
#define http_contentgenerator_hpp_included_

#include <ctime>
#include <string>
#include <vector>
#include <memory>
#include <exception>

#include "./sink.hpp"
#include "./request.hpp"

namespace http {

class ContentGenerator {
public:
    typedef std::shared_ptr<ContentGenerator> pointer;

    virtual ~ContentGenerator() {}
    void generate(const Request &request
                  , const ServerSink::pointer &sink);

private:
    virtual void generate_impl(const Request &request
                               , const ServerSink::pointer &sink) = 0;
};

// inlines

inline void ContentGenerator::generate(const Request &request
                                       , const ServerSink::pointer &sink)
{
    return generate_impl(request, sink);
}

} // namespace http

#endif // http_contentgenerator_hpp_included_
