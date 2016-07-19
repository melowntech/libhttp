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
                              , const FileInfo &stat, bool);

    virtual void error_impl(const std::exception_ptr &exc);

    virtual void seeOther_impl(const std::string &url);

    std::shared_ptr<QuerySink> owner;
    ResourceFetcher::Query *query;
};

struct QuerySinkBase {
    std::shared_ptr<ResourceFetcher::MultiQuery> query_;
    ResourceFetcher::Done done_;

    QuerySinkBase(const ResourceFetcher::MultiQuery &query
                  , const ResourceFetcher::Done &done)
        : query_(std::make_shared<ResourceFetcher::MultiQuery>(query))
        , done_(done)
    {}
};

class QuerySink
    : private QuerySinkBase
    , public SingleQuerySink
{
public:
    typedef std::shared_ptr<QuerySink> pointer;

    QuerySink(const ResourceFetcher::MultiQuery &query
              , asio::io_service *ios
              , const ResourceFetcher::Done &done)
        : QuerySinkBase(query, done)
        , SingleQuerySink(((query.size() == 1)
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
                    ios_->post([done_, query_]() { done_(*query_); });
                } else {
                    done_(*query_);
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
        auto &q(*sink->query_);
        if (q.empty()) {
            // ehm...
            if (sink->ios_) {
                sink->ios_->post([=]() { sink->done_(q); });
            } else {
                sink->done_(q);
            }
            return;
        }

        if (q.size() == 1) {
            // single query -> use sink
            contentFetcher.fetch(q.front().location(), sink);
            return;
        }

        // multiquery -> use multiple queries
        for (auto &query : q) {
            contentFetcher.fetch
                (query.location()
                 , std::make_shared<SingleQuerySink>(sink, query));
        }
    }

private:
    asio::io_service *ios_;
    std::atomic<int> queriesLeft_;
};

void SingleQuerySink::content_impl(const void *data, std::size_t size
                                   , const FileInfo &stat, bool)
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

void SingleQuerySink::seeOther_impl(const std::string &url)
{
    query->redirect(url);
    owner->ping();
}

} // namespace

void ResourceFetcher::perform_impl(const MultiQuery &query, const Done &done)
    const
{
    QuerySink::fetch
        (std::make_shared<QuerySink>(query, queryIos_, done)
         , contentFetcher_);
}

} // namespace http
