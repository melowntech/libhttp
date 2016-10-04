#ifndef http_error_hpp_included_
#define http_error_hpp_included_

#include <ctime>
#include <string>
#include <memory>
#include <stdexcept>

#include "utility/httpcode.hpp"

namespace http {

struct Error : std::runtime_error {
    Error(const std::string &message) : std::runtime_error(message) {}
};

typedef utility::HttpError HttpError;

#define HTTP_DEFINE_ERROR(NAME)                                         \
    struct NAME : HttpError {                                           \
        NAME(const std::string &message)                                \
            : HttpError(make_error_code(utility::HttpCode::NAME)        \
                                 , message) {}                          \
    }

HTTP_DEFINE_ERROR(NotAllowed);
HTTP_DEFINE_ERROR(NotFound);
HTTP_DEFINE_ERROR(NotAuthorized);
HTTP_DEFINE_ERROR(Forbidden);
HTTP_DEFINE_ERROR(BadRequest);
HTTP_DEFINE_ERROR(ServiceUnavailable);
HTTP_DEFINE_ERROR(InternalServerError);
HTTP_DEFINE_ERROR(NotModified);
HTTP_DEFINE_ERROR(RequestAborted);

#undef HTTP_DEFINE_ERROR

} // namespace http

#endif // http_error_hpp_included_
