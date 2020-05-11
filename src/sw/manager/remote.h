/*
 * SW - Build System and Package Manager
 * Copyright (C) 2016-2018 Egor Pugin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

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

    Remote(const String &name, const String &url);

    std::unique_ptr<Api> getApi() const;
    ApiType getApiType() const { return type; }

    bool isDisabled() const { return disabled; }

private:
    GrpcChannel getGrpcChannel() const;

    friend struct ProtobufApi;
};

std::vector<std::shared_ptr<Remote>> get_default_remotes();

}
