// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "enums.h"

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

    String raw_url;
    SomeFlags flags;
    String location; // other type?

    String getUrl(const Package &pkg) const;

    // returns hash
    bool downloadPackage(const Package &d, const path &fn, String &dl_hash) const;
};

using DataSources = std::vector<DataSource>;

struct Api;

struct Remote
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

    using Url = String;
    using SourcesUrls = std::vector<Url>;

    String name;
    Url url;

    std::map<String, Publisher> publishers;
    bool secure = true;
    ApiType type = ApiType::Protobuf;

    std::unique_ptr<Api> getApi() const;
    ApiType getApiType() const { return type; }

private:
    GrpcChannel getGrpcChannel() const;

    friend struct ProtobufApi;
};

using Remotes = std::vector<Remote>;
Remotes get_default_remotes();

}
