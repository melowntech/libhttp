#ifndef http_contentgenerator_hpp_included_
#define http_contentgenerator_hpp_included_

#include <ctime>
#include <string>
#include <vector>
#include <memory>
#include <exception>

#include "./sink.hpp"

namespace http {

class ContentGenerator {
public:
    typedef std::shared_ptr<ContentGenerator> pointer;

    virtual ~ContentGenerator() {}
    void generate(const std::string &location
                  , const ServerSink::pointer &sink);

private:
    virtual void generate_impl(const std::string &location
                               , const ServerSink::pointer &sink) = 0;
};

// inlines

inline void ContentGenerator::generate(const std::string &location
                                       , const ServerSink::pointer &sink)
{
    return generate_impl(location, sink);
}

} // namespace http

#endif // http_contentgenerator_hpp_included_
