#ifndef http_constants_hpp_included_
#define http_constants_hpp_included_

#include <ctime>

namespace http {

namespace constants {
    static constexpr std::time_t mustRevalidate = -2;
    static constexpr std::time_t cacheUnspecified = -1;
} // namespace constants

} // namespace http

#endif // http_constants_hpp_included_

