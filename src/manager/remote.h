// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "enums.h"
#include "filesystem.h"
#include "http.h"

#include <functional>

#define DEFAULT_REMOTE_NAME "origin"

namespace grpc
{

class Channel;

}

namespace sw
{

struct Package;
struct PackageId;

struct DataSource
{
    enum Flags
    {
        fHasPrivatePackages     =   0,
        fHasPrebuiltPackages    =   1,
    };

    String raw_url;
    SomeFlags flags;
    String location; // other type?

    String getUrl(const PackageId &pkg) const;

    bool downloadPackage(const Package &d, const String &hash, const path &fn, bool try_only_first = false) const;
};

using DataSources = std::vector<DataSource>;

struct Remote
{
    using Url = String;
    using SourcesUrls = std::vector<Url>;

    String name;
    Url url;
    String user;
    String token;

    mutable std::shared_ptr<grpc::Channel> channel;

    std::shared_ptr<grpc::Channel> getGrpcChannel() const;
};

using Remotes = std::vector<Remote>;
Remotes get_default_remotes();

}
