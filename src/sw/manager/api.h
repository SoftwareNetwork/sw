// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "enums.h"
#include "package.h"
#include "package_data.h"
#include "remote.h"
#include "version.h"

#undef ERROR
#include <api.grpc.pb.h>
#undef strtoll
#undef strtoull

namespace sw
{

struct Api
{
    int deadline_secs = 5;

    Api(const Remote &r);

    void addDownloads(const std::set<int64_t> &);
    void addClientCall();
    //IdDependencies resolvePackages(const UnresolvedPackages &);

    void addVersion(PackagePath prefix, const PackageDescriptionMap &pkgs, const String &script);
    void addVersion(const PackagePath &prefix, const String &script);
    void addVersion(PackagePath p, const Version &vnew, const std::optional<Version> &vold = {});
    void updateVersion(PackagePath p, const Version &v);
    void removeVersion(PackagePath p, const Version &v);

    void getNotifications(int n = 10);
    void clearNotifications();

private:
    const Remote &r;
    std::unique_ptr<api::ApiService::Stub> api_;
    std::unique_ptr<api::UserService::Stub> user_;

    std::unique_ptr<grpc::ClientContext> getContext();
    std::unique_ptr<grpc::ClientContext> getContextWithAuth();
};

}
