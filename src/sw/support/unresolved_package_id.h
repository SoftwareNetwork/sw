// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "settings.h"
#include "unresolved_package_name.h"

#include <unordered_set>

namespace sw
{

struct SW_SUPPORT_API UnresolvedPackageId
{
    UnresolvedPackageId(const PackageName &, const PackageSettings & = {});
    UnresolvedPackageId(const UnresolvedPackageName &, const PackageSettings & = {});

    const UnresolvedPackageName &getName() const { return name; }

    PackageSettings &getSettings() { return settings; }
    const PackageSettings &getSettings() const { return settings; }

    bool operator==(const UnresolvedPackageId &rhs) const { return std::tie(name, settings) == std::tie(rhs.name, rhs.settings); }

private:
    UnresolvedPackageName name;
    PackageSettings settings;
};

struct SW_SUPPORT_API UnresolvedPackageIdFull
{
    UnresolvedPackageIdFull(const PackageName &, const PackageSettings & = {});
    UnresolvedPackageIdFull(const UnresolvedPackageName &, const PackageSettings & = {});

    const UnresolvedPackageName &getName() const { return name; }

    PackageSettings &getSettings() { return settings; }
    const PackageSettings &getSettings() const { return settings; }

    bool operator==(const UnresolvedPackageIdFull &rhs) const { return std::tie(name, settings) == std::tie(rhs.name, rhs.settings); }

private:
    UnresolvedPackageName name;
    PackageSettings settings;
};

}

namespace std
{

template<> struct hash<::sw::UnresolvedPackageIdFull>
{
    size_t operator()(const ::sw::UnresolvedPackageIdFull &p) const
    {
        auto h = std::hash<::sw::UnresolvedPackageName>()(p.getName());
        return hash_combine(h, (uint64_t)p.getSettings().getHash());
    }
};

}
