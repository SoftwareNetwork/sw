// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#include "server.h"

#include <sw/builder/command.h>

#include <grpcpp/grpcpp.h>
#include <primitives/exceptions.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "builder.distributed.server");

// move this just into builder itself?

namespace sw::builder::distributed
{

DEFINE_SERVICE_METHOD(DistributedBuildService, ExecuteCommand, ::sw::api::build::Command, ::sw::api::build::CommandResult)
{
    // fan mode: send to workers in round robin mode

    SW_UNIMPLEMENTED;

    Command c;
    //c.in.file = fs::u8path(request->in().file());
    //c.in.file = fs::u8path(request->in().file());
    //c.in.file = fs::u8path(request->in().file());

    c.execute();

    GRPC_RETURN_OK();
}

Server::Server()
{
}

Server::~Server()
{
}

void Server::start(const String &server_address/*, const String &cert*/)
{
    grpc::SslServerCredentialsOptions ssl_options;
    //if (!cert.empty())
        //ssl_options.pem_key_cert_pairs.push_back({ read_file("server.key"), read_file("server.crt") });

    grpc::ServerBuilder builder;
    //if (sw::settings().grpc_use_ssl)
        //builder.AddListeningPort(server_address, grpc::SslServerCredentials(ssl_options));
    //else
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());

    builder.RegisterService(&dbs);
    server = builder.BuildAndStart();
    if (!server)
        throw SW_RUNTIME_ERROR("Cannot start grpc server");
}

void Server::wait()
{
    if (!server)
        throw SW_RUNTIME_ERROR("Server not started");
    server->Wait();
}

void Server::stop()
{
    if (!server)
        throw SW_RUNTIME_ERROR("Server not started");
    server->Shutdown();
}

}
