// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "package.h"

#include <sw/support/package_data.h>
#include <sw/support/specification.h>

namespace sw
{

struct IStorage;
struct ResolveRequest;

struct Api
{
    //using ResolveResult = std::unordered_map<UnresolvedPackage, PackagePtr>;

    virtual ~Api() = 0;

    virtual bool resolve(
        ResolveRequest &rr,
        std::unordered_map<PackageId, PackageData> &data, const IStorage &) const = 0;
    /*virtual ResolveResult resolvePackages(
        const UnresolvedPackages &pkgs,
        UnresolvedPackages &unresolved_pkgs,
        std::unordered_map<PackageId, PackageData> &data, const IStorage &) const = 0;*/
    virtual void addVersion(const PackagePath &prefix, const PackageDescriptionMap &pkgs, const SpecificationFiles &) const = 0;
};

}
