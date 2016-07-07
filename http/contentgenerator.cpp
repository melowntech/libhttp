#include "./sink.hpp"
#include "./error.hpp"

namespace http {

void SinkBase::error()
{
    error_impl(std::current_exception());
}

void ServerSink::checkAborted() const
{
    if (checkAborted_impl()) {
        throw RequestAborted("Request aborted.");
    }
}

} // namespace http
