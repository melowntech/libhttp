#ifndef http_http_hpp_included_
#define http_http_hpp_included_

#include <memory>
#include <string>

#include "utility/tcpendpoint.hpp"

#include "./contentgenerator.hpp"
#include "./contentfetcher.hpp"

namespace http {

class Http {
public:
    /** Simple server-side interface: listen at given endpoint and start
     *  machinery right away.
     *
     *  NB: contentGenerator must survive this object
     */
    Http(const utility::TcpEndpoint &listen
         , unsigned int threadCount
         , ContentGenerator &contentGenerator);

    /** Create HTTP machiner and do nothing.
     */
    Http();

    /** Listen at given endpoint and as content generator to generate replies to
     *  received requests.
     *  Returned value can be different if port was 0 (i.e. listen at any
     *  available port)
     *
     * \param listen address where to listen
     * \param contentGenerator request handler
     * \return real listening endpoint
     */
    utility::TcpEndpoint
    listen(const utility::TcpEndpoint &listen
           , const ContentGenerator::pointer &contentGenerator);

    /** Listen at given endpoint and as content generator to generate replies to
     *  received requests.
     *
     *  NB: contentGenerator must survive this object
     *
     *  Returned value can be different if port was 0 (i.e. listen at any
     *  available port)
     *
     * \param listen address where to listen
     * \param contentGenerator request handler
     * \return real listening endpoint
     */
    utility::TcpEndpoint listen(const utility::TcpEndpoint &listen
                                , ContentGenerator &contentGenerator);

    /** Start server-side processing machinery.
     */
    void startServer(unsigned int threadCount);

    /** Start client-side processing machinery.
     */
    void startClient(unsigned int threadCount);

    /** Stop processing machinery.
     */
    void stop();

    /** Set server header.
     */
    void serverHeader(const std::string &value);

    /** Returns content fetcher interface.
     */
    ContentFetcher& fetcher();

    struct Detail;
    friend struct Detail;

private:
    std::shared_ptr<Detail> detail_;
    Detail& detail() { return *detail_; }
    const Detail& detail() const { return *detail_; }
};

} // namespace http

#endif // http_http_hpp_included_
