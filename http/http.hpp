#ifndef http_http_hpp_included_
#define http_http_hpp_included_

#include <memory>
#include <string>

#include "utility/tcpendpoint.hpp"

#include "contentgenerator.hpp"
#include "contentfetcher.hpp"

namespace http {

class Http : public ContentFetcher {
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
     */
    void listen(const utility::TcpEndpoint &listen
                , const ContentGenerator::pointer &contentGenerator);

    /** Listen at given endpoint and as content generator to generate replies to
     *  received requests.
     *
     *  NB: contentGenerator must survive this object
     */
    void listen(const utility::TcpEndpoint &listen
                , ContentGenerator &contentGenerator);

    /** Start processing machinery.
     */
    void start(unsigned int threadCount);

    /** Stop processing machinery.
     */
    void stop();

    struct Detail;

private:
    void fetch_impl(const utility::Uri &location
                    , const ClientSink::pointer &sink);

    std::shared_ptr<Detail> detail_;
    Detail& detail() { return *detail_; }
    const Detail& detail() const { return *detail_; }
};

} // namespace http

#endif // http_http_hpp_included_
