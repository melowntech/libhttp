
#include <vector>
#include <fstream>
#include <functional>
#include <unistd.h> // usleep
#include <dbglog/dbglog.hpp>

#include <http/http.hpp>
#include <http/resourcefetcher.hpp>

std::atomic<int> active;
std::atomic<int> succeded;
std::atomic<int> finished;
std::atomic<int> started;

class Task
{
public:
    Task(const std::string &url) : query(url)
    {
        active++;
        started++;
        query.timeout(5000);
    }

    ~Task()
    {
        active--;
    }

    void done(http::ResourceFetcher::MultiQuery &&queries)
    {
        finished++;
        http::ResourceFetcher::Query &q = *queries.begin();
        if (q.exc())
        {
            try
            {
                std::rethrow_exception(q.exc());
            }
            catch(std::exception &e)
            {
                LOG(err3) << "exception: " << e.what();
            }
            catch(...)
            {
                LOG(err3) << "unknown exception";
            }
        }
        else if (q.valid())
        {
            succeded++;
            const http::ResourceFetcher::Query::Body &body = q.get();
            LOG(info3) << "Downloaded: '" << q.location()
                       << "', size: " << body.data.length();
        }
        else
            LOG(err3) << "Failed: " << q.location()
                      << ", http code: " << q.ec().value();
    }

    http::ResourceFetcher::Query query;
};

int main(int argc, const char *args[])
{
    LOG(info4) << "Usage: " << args[0]
               << " [target-downloads-count [urls-file-path]]";

    unsigned long targetDownloads = 100;
    std::vector<std::string> urls({
        "https://www.melown.com/",
        "https://www.melown.com/tutorials.html",
        "https://www.melown.com/blog.html",
    });

    if (argc > 1) {
        std::stringstream s(args[1]);
        s >> targetDownloads;
    }
    LOG(info4) << "Target number of downloads: " << targetDownloads << ".";

    if (argc > 2) {
        LOG(info4) << "Loading urls from file.";
        std::string line;
        std::ifstream f(args[2]);
        if (f.is_open()) {
            urls.clear();
            while (std::getline(f,line)) {
                if (!line.empty()) {
                    urls.push_back(line);
                }
            }
            f.close();
        } else {
            LOG(warn4) << "Failed to open specified file.";
        }
    }
    LOG(info4) << "Will download from " << urls.size() << " urls.";

    http::Http htt;
    http::ResourceFetcher fetcher(htt.fetcher());

    {
        http::ContentFetcher::Options options;
        options.maxTotalConections = 10;
        options.pipelining = 2;
        htt.startClient(2, &options);
    }

    for (unsigned long i = 0; i < targetDownloads; i++)
    {
        while (active > 25)
            usleep(1000);
        auto t = std::make_shared<Task>(urls[rand() % urls.size()]);
        fetcher.perform(t->query, std::bind(&Task::done, t,
                                            std::placeholders::_1));
    }

    LOG(info3) << "Waiting for threads to stop.";
    htt.stop();

    LOG(info4) << "Client stopped"
               << ", downloads started: " << started
               << ", finished: " << finished
               << ", succeded: " << succeded
               << ".";

    return 0;
}


