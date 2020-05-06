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
