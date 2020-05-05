// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
