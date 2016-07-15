#ifndef http_detail_types_hpp_included_
#define http_detail_types_hpp_included_

#include <memory>
#include <string>
#include <vector>

#include "utility/enum-io.hpp"

namespace http { namespace detail {

enum class StatusCode : int {
    OK = 200

    , Found = 302
    , NotModified = 304

    , BadRequest = 400
    , NotFound = 404
    , NotAllowed = 405

    , InternalServerError = 500
    , ServiceUnavailable = 503
};

UTILITY_GENERATE_ENUM_IO(StatusCode,
    ((OK)("OK"))
    ((Found)("Found" ))
    ((NotModified)("Not Modified"))
    ((BadRequest)("Bad Request"))
    ((NotFound)("Not Found"))
    ((NotAllowed)("Not Allowed"))
    ((InternalServerError)("Internal Server Error"))
    ((ServiceUnavailable)("Service Unavailabe"))
)

struct Header {
    std::string name;
    std::string value;

    typedef std::vector<Header> list;

    Header() {}
    Header(const std::string &name, const std::string &value)
        : name(name), value(value) {}
};

struct Request {
    std::string method;
    std::string uri;
    std::string version;
    Header::list headers;
    std::size_t lines;

    enum class State { reading, ready, broken };
    State state;

    typedef std::vector<Request> list;

    Request() { clear(); }

    void makeReady() { state = State::ready; }
    void makeBroken() { state = State::broken; }

    void clear() {
        method.clear();
        uri.clear();
        version = "HTTP/1.1";
        headers.clear();
        lines = 0;
        state = State::reading;
    }
};

struct Response {
    StatusCode code;
    Header::list headers;
    std::string reason;
    bool close;

    Response(StatusCode code = StatusCode::OK)
        : code(code), close(false)
    {}

    int numericCode() const { return static_cast<int>(code); }
};

std::string formatHttpDate(std::time_t time);

} } // namespace http::detail

#endif // http_detail_types_hpp_included_
