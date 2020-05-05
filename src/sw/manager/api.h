// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "package.h"

#include <sw/support/package_data.h>
#include <sw/support/specification.h>

namespace sw
{

struct IStorage;

struct Api
{
    virtual ~Api() = 0;

    virtual std::unordered_map<UnresolvedPackage, PackagePtr>
    resolvePackages(
        const UnresolvedPackages &pkgs,
        UnresolvedPackages &unresolved_pkgs,
        std::unordered_map<PackageId, PackageData> &data, const IStorage &) const = 0;
    virtual void addVersion(const PackagePath &prefix, const PackageDescriptionMap &pkgs, const SpecificationFiles &) const = 0;
};

}
