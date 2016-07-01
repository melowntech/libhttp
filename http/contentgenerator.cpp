#include "./contentgenerator.hpp"
#include "./error.hpp"

namespace http {

void Sink::error()
{
    error_impl(std::current_exception());
}

void Sink::checkAborted() const
{
    if (checkAborted_impl()) {
        throw RequestAborted("Request aborted.");
    }
}

} // namespace http
