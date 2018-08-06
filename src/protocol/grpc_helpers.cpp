#include "grpc_helpers.h"

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "protocol");

namespace sw
{

// namespace api?

CallResult check_result(
    const grpc::Status &status,
    const grpc::ClientContext &context,
    const std::string &method,
    bool throws
)
{
    CallResult r;

    if (!status.ok())
    {
        auto err = "Method '" + method + "': RPC failed: " + std::to_string(status.error_code()) + ": " + status.error_message();
        if (throws)
            throw std::runtime_error(err);
        else
            LOG_ERROR(logger, err);
        r.ec = std::make_error_code((std::errc)status.error_code());
        r.message = status.error_message();
        return r;
    }

    auto result = get_metadata_variable(context.GetServerTrailingMetadata(), "ec");
    if (result.empty())
    {
        auto err = "Method '" + method + "': missing error code";
        if (throws)
            throw std::runtime_error(err);
        else
            LOG_DEBUG(logger, err);
        r.ec = std::make_error_code((std::errc)1);
        return r;
    }
    auto ec = std::stoi(result.data());
    if (ec)
    {
        auto message = get_metadata_variable(context.GetServerTrailingMetadata(), "message");
        auto err = "Method '" + method + "' returned error: ec = " + result + ", message: " + message;
        if (throws)
            throw std::runtime_error(err);
        else
            LOG_DEBUG(logger, err);
        r.ec = std::make_error_code((std::errc)ec);
        r.message = message;
    }

    return r;
}

std::string get_metadata_variable(const std::multimap<grpc::string_ref, grpc::string_ref> &m, const std::string &key)
{
    auto i = m.find(key);
    if (i == m.end())
        return {};
    return std::string(std::string_view(i->second.data(), i->second.size()));
}

}
