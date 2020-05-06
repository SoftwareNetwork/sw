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

#include "remote.h"

#include <grpcpp/grpcpp.h>

#undef ERROR
#include <sw/protocol/api.grpc.pb.h>
#undef strtoll
#undef strtoull

namespace sw
{

struct Remote;

struct ProtobufApi : Api
{
    ProtobufApi(const Remote &);

    std::unordered_map<UnresolvedPackage, PackagePtr> resolvePackages(const UnresolvedPackages &pkgs, UnresolvedPackages &unresolved_pkgs,
        std::unordered_map<PackageId, PackageData> &data, const IStorage &) const override;
    void addVersion(const PackagePath &prefix, const PackageDescriptionMap &pkgs, const SpecificationFiles &) const override;

    void addVersion(const PackagePath &prefix, const String &script);
    void addVersion(PackagePath p, const Version &vnew, const std::optional<Version> &vold = {});
    void updateVersion(PackagePath p, const Version &v);
    void removeVersion(PackagePath p, const Version &v);

    void getNotifications(int n = 10);
    void clearNotifications();

private:
    const Remote &r;
    GrpcChannel c;
    std::unique_ptr<api::ApiService::Stub> api_;
    std::unique_ptr<api::UserService::Stub> user_;

    std::unique_ptr<grpc::ClientContext> getContext() const;
    std::unique_ptr<grpc::ClientContext> getContextWithAuth() const;
};

}
