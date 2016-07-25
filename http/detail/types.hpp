#ifndef http_detail_types_hpp_included_
#define http_detail_types_hpp_included_

#include <memory>
#include <string>
#include <vector>

#include "utility/enum-io.hpp"

#include "../request.hpp"

namespace http { namespace detail {

typedef utility::HttpCode StatusCode;

typedef http::Header Header;

struct Request : http::Request {
    std::string method;
    std::string version;
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
