// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <sw/protocol/build.grpc.pb.h>
#include <sw/protocol/grpc_helpers.h>

#include <grpcpp/server.h>
#include <primitives/string.h>

#include <memory>
#include <vector>

namespace sw::builder::distributed
{

class DistributedBuildServiceImpl : public ::sw::api::build::DistributedBuildService::Service
{
    DECLARE_SERVICE_METHOD(ExecuteCommand, ::sw::api::build::Command, ::sw::api::build::CommandResult);
};

struct SW_BUILDER_DISTRIBUTED_API Client
{

};

struct SW_BUILDER_DISTRIBUTED_API Session
{
    std::unique_ptr<Client> client;
};

struct SW_BUILDER_DISTRIBUTED_API Worker
{

};

struct SW_BUILDER_DISTRIBUTED_API Server
{
    std::unique_ptr<grpc::Server> server;
    DistributedBuildServiceImpl dbs;
    std::vector<std::unique_ptr<Session>> sessions;
    std::vector<std::unique_ptr<Worker>> workers;

    Server();
    ~Server();

    void start(const String &endpoint/*, const String &cert = {}*/);
    void wait();
    void stop();
};

}
