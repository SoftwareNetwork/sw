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

String default_source_provider(const Package &);

struct Remote
{
    using Url = String;
    using SourcesUrls = std::vector<Url>;
    using SourceUrlProvider = std::function<String(const Remote &, const Package &)>;

    String name;

    Url url;
    String data_dir;

    String user;
    String token;

    // own data
    // sources
    std::vector<SourceUrlProvider> primary_sources;
    SourceUrlProvider default_source{ &Remote::default_source_provider };
    std::vector<SourceUrlProvider> additional_sources;

    std::shared_ptr<grpc::Channel> getGrpcChannel() const;
    bool downloadPackage(const Package &d, const String &hash, const path &fn, bool try_only_first = false) const;

public:
    String default_source_provider(const Package &) const;
    String cppan2_source_provider(const Package &) const;
    String github_source_provider(const Package &) const;

private:
    mutable std::shared_ptr<grpc::Channel> channel;
};

using Remotes = std::vector<Remote>;
Remotes get_default_remotes();

}
