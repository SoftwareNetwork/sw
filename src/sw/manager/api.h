// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "enums.h"
#include "package.h"
#include "package_data.h"
#include "version.h"

namespace sw
{

struct Api
{
    struct RemotePackageData : PackageId, PackageData
    {
        using PackageId::PackageId;

        db::PackageVersionId id;
        std::unordered_set<db::PackageVersionId> deps;
    };

    using IdDependencies = std::unordered_map<db::PackageVersionId, RemotePackageData>;

    virtual ~Api() = 0;

    virtual IdDependencies resolvePackages(const UnresolvedPackages &) const = 0;
    virtual void addVersion(PackagePath prefix, const PackageDescriptionMap &pkgs, const String &script) const = 0;
};

}
