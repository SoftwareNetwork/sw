// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

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

struct DataProvider
{
    String raw_url;

    String getUrl(const PackageId &pkg) const;

    bool downloadPackage(const Package &d, const String &hash, const path &fn, bool try_only_first = false) const;
};

using DataProviders = std::vector<DataProvider>;

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
