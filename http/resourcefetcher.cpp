#include "dbglog/dbglog.hpp"

#include "./contentfetcher.hpp"
#include "./resourcefetcher.hpp"

namespace asio = boost::asio;

namespace http {

namespace {
class QuerySink;

struct SingleQuerySink : public http::ClientSink {
    typedef std::shared_ptr<SingleQuerySink> pointer;

    SingleQuerySink(const std::shared_ptr<QuerySink> &owner
                    , ResourceFetcher::Query &query)
        : owner(owner), query(&query)
    {}

    virtual void content_impl(const void *data, std::size_t size
                              , const FileInfo &stat, bool
                              , const Header::list *headers);

    virtual void error_impl(const std::exception_ptr &exc);

    virtual void error_impl(const std::error_code &ec
                            , const std::string &message);

    virtual void seeOther_impl(const std::string &url);

    std::shared_ptr<QuerySink> owner;
    ResourceFetcher::Query *query;
};

struct QuerySinkBase {
    std::shared_ptr<ResourceFetcher::MultiQuery> query_;
    ResourceFetcher::Done done_;

    QuerySinkBase(ResourceFetcher::MultiQuery &&query
                  , const ResourceFetcher::Done &done)
        : query_(std::make_shared<ResourceFetcher::MultiQuery>
                 (std::move(query)))
        , done_(done)
    {}
};

class QuerySink
    : private QuerySinkBase
    , public SingleQuerySink
{
public:
    typedef std::shared_ptr<QuerySink> pointer;

    QuerySink(ResourceFetcher::MultiQuery &&query
              , asio::io_service *ios
              , const ResourceFetcher::Done &done)
        : QuerySinkBase(std::move(query), done)
        , SingleQuerySink(((query_->size() == 1)
                           ? std::shared_ptr<QuerySink>(this, [](void*){})
                           : pointer())
                          , query_->front())
        , ios_(ios)
        , queriesLeft_(query_->size())
    {}

    void ping() {
        // decrement
        if (!--queriesLeft_) {
            try {
                LOG(info2) << "All subqueries finished.";
                if (ios_) {
                    ios_->post([done_, query_]() {
                            done_(std::move(*query_));
                        });
                } else {
                    done_(std::move(*query_));
                }
            } catch (const std::exception &e) {
                LOG(err2)
                    << "Resource(s) fetch callback failed: <"
                    << e.what() << ">.";
            }
        }
    }

    /** Fetch entry point.
     */
    static void fetch(const pointer &sink
                      , http::ContentFetcher &contentFetcher)
    {
        if (sink->query_->empty()) {
            auto &q(sink->query_);
            // ehm...
            if (sink->ios_) {
                sink->ios_->post([=]() { sink->done_(std::move(*q)); });
            } else {
                sink->done_(std::move(*q));
            }
            return;
        }

        auto &q(*sink->query_);
        if (q.size() == 1) {
            // single query -> use sink
            ContentFetcher::RequestOptions options;
            const auto &query(q.front());
            options.reuse = query.reuse();
            options.timeout = query.timeout();
            contentFetcher.fetch(query.location(), sink, options);
            return;
        }

        // multiquery -> use multiple queries
        for (auto &query : q) {
            ContentFetcher::RequestOptions options;
            options.reuse = query.reuse();
            options.timeout = query.timeout();
            contentFetcher.fetch
                (query.location()
                 , std::make_shared<SingleQuerySink>(sink, query), options);
        }
    }

private:
    asio::io_service *ios_;
    std::atomic<int> queriesLeft_;
};

void SingleQuerySink::content_impl(const void *data, std::size_t size
                                   , const FileInfo &stat, bool
                                   , const Header::list*)
{
    query->set(stat.lastModified, stat.expires, data, size
               , stat.contentType);
    owner->ping();
}

void SingleQuerySink::error_impl(const std::exception_ptr &exc)
{
    query->error(exc);
    owner->ping();
}

void SingleQuerySink::error_impl(const std::error_code &ec
                                 , const std::string&)
{
    query->error(ec);
    owner->ping();
}

void SingleQuerySink::seeOther_impl(const std::string &url)
{
    query->redirect(url);
    owner->ping();
}

} // namespace

void ResourceFetcher::perform_impl(MultiQuery query, const Done &done)
    const
{
    QuerySink::fetch
        (std::make_shared<QuerySink>(std::move(query), queryIos_, done)
         , contentFetcher_);
}

} // namespace http
