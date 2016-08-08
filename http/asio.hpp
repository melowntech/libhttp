#ifndef http_asio_hpp_included_
#define http_asio_hpp_included_

#include <boost/asio.hpp>

#include "./http.hpp"

namespace http {

/** Returns internal io-service from http. Can be used to plug own asynchronous
 *  machinery into http's thread pool.
 */
boost::asio::io_service& ioService(const Http &http);

} // namespace http

#endif // http_asio_hpp_included_
