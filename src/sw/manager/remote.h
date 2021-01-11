// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <sw/support/enums.h>
#include <sw/support/filesystem.h>

#include <functional>
#include <optional>

#define DEFAULT_REMOTE_NAME "origin"

namespace grpc
{

class ChannelInterface;

}

namespace sw
{

using GrpcChannel = std::shared_ptr<grpc::ChannelInterface>;

struct Package;
struct PackageId;

struct DataSource
{
    enum Flags
    {
        fDisabled               =   0,
        fHasPrivatePackages     =   1,
        fHasPrebuiltPackages    =   2,
    };

    enum Type
    {
        tHttp   = 0,
    };

    String raw_url;
    int type = tHttp;
    SomeFlags flags;
    String location; // other type?

    String getUrl(const Package &pkg) const;

    // returns hash
    bool downloadPackage(const Package &d, const path &fn, String &dl_hash) const;
};

using DataSources = std::vector<DataSource>;

struct Api;

struct SW_MANAGER_API Remote
{
    enum class ApiType
    {
        Protobuf = 0,
        // msgpack
        // json rpc
        // soap
        // ...
        // etc.
    };

    struct Publisher
    {
        String name;
        String token;
    };

    struct DatabaseInformation
    {
        // from spec
        String git_repo_url;
        String url;
        String local_dir;
        String version_root_url;

        // other
        mutable int version = -1;

        String getVersionUrl() const;
        int getVersion() const;
    };

    using Url = String;
    using SourcesUrls = std::vector<Url>;

    String name;
    Url url;
    Url api_url;
    DatabaseInformation db;
    DataSources dss;
    std::map<String, Publisher> publishers;
    bool secure = true;
    // access pubkey
    // db pubkey
    // single pubkey?
    ApiType type = ApiType::Protobuf;
    bool disabled = false;

    Remote(const String &name, const String &url, bool allow_network);

    std::unique_ptr<Api> getApi() const;
    ApiType getApiType() const { return type; }

    bool isDisabled() const { return disabled; }

private:
    GrpcChannel getGrpcChannel() const;

    friend struct ProtobufApi;
};

std::vector<std::shared_ptr<Remote>> get_default_remotes(bool allow_network);

}
