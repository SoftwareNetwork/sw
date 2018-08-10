#pragma once

#include <grpcpp/impl/codegen/client_context.h>
#include <grpcpp/impl/codegen/status.h>

#include <system_error>

#define GRPC_CALL_INTERNAL(svc, m, resptype, t) \
    resptype response;                          \
    check_result(svc->m(context.get(), request, &response), *context, #m, t)
#define GRPC_CALL(svc, m, resptype) GRPC_CALL_INTERNAL(svc, m, resptype, false)
#define GRPC_CALL_THROWS(svc, m, resptype) GRPC_CALL_INTERNAL(svc, m, resptype, true)

#define SW_GRPC_METADATA_AUTH_USER "auth-user"
#define SW_GRPC_METADATA_AUTH_TOKEN "auth-token"
#define SW_GRPC_METADATA_CLIENT_VERSION "client-version"

namespace sw
{

struct CallResult
{
    std::error_code ec;
    std::string message;

    operator bool() const { return !ec; }
};

CallResult check_result(
    const grpc::Status &status,
    const grpc::ClientContext &context,
    const std::string &method,
    bool throws = false
);

std::string get_metadata_variable(const std::multimap<grpc::string_ref, grpc::string_ref> &metadata, const std::string &key);

}
