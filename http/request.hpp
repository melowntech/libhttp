#ifndef http_request_hpp_included_
#define http_request_hpp_included_

#include <string>
#include <vector>

namespace http {

struct Header {
    std::string name;
    std::string value;

    typedef std::vector<Header> list;

    Header() {}
    Header(const std::string &name, const std::string &value)
        : name(name), value(value) {}
};

struct Request {
    /** Uri as received from client
     */
    std::string uri;

    /** Cleaned-up path from uri
     */
    std::string path;

    /** Query string from uri
     */
    std::string query;

    Header::list headers;

    bool hasHeader(const std::string &name) const;

    const std::string* getHeader(const std::string &name) const;
};

inline bool Request::hasHeader(const std::string &name) const
{
    return getHeader(name);
}

} // namespace http

#endif // http_request_hpp_included_
