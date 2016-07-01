#ifndef http_error_hpp_included_
#define http_error_hpp_included_

#include <ctime>
#include <string>
#include <memory>
#include <stdexcept>

namespace http {

struct Error : std::runtime_error {
    Error(const std::string &message) : std::runtime_error(message) {}
};

struct ProtocolError : Error {
    ProtocolError(const std::string &message) : Error(message) {}
};

#define HTTP_DEFINE_ERROR(NAME)                                         \
    struct NAME : ProtocolError {                                       \
        NAME(const std::string &message) : ProtocolError(message) {}    \
    }

HTTP_DEFINE_ERROR(NotAllowed);
HTTP_DEFINE_ERROR(NotFound);
HTTP_DEFINE_ERROR(Unavailable);
HTTP_DEFINE_ERROR(InternalError);
HTTP_DEFINE_ERROR(RequestAborted);

#undef HTTP_DEFINE_ERROR

} // namespace http

#endif // http_error_hpp_included_
