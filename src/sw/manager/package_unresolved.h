// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "package_path.h"
#include "version.h"

#include <unordered_set>

namespace sw
{

struct PackageId;

struct SW_MANAGER_API UnresolvedPackage
{
    PackagePath ppath;
    VersionRange range;

    UnresolvedPackage() = default;
    UnresolvedPackage(const PackagePath &p, const VersionRange &r);
    UnresolvedPackage(const String &s);
    UnresolvedPackage(const PackageId &);

    UnresolvedPackage &operator=(const String &s);

    PackagePath getPath() const { return ppath; }
    VersionRange getRange() const { return range; }

    std::optional<PackageId> toPackageId() const;
    String toString(const String &delim = "-") const;
    bool canBe(const PackageId &id) const;

    bool operator<(const UnresolvedPackage &rhs) const { return std::tie(ppath, range) < std::tie(rhs.ppath, rhs.range); }
    bool operator==(const UnresolvedPackage &rhs) const { return std::tie(ppath, range) == std::tie(rhs.ppath, rhs.range); }
    bool operator!=(const UnresolvedPackage &rhs) const { return !operator==(rhs); }
};

using UnresolvedPackages = std::unordered_set<UnresolvedPackage>;

SW_MANAGER_API
UnresolvedPackage extractFromString(const String &target);

}

namespace std
{

template<> struct hash<::sw::UnresolvedPackage>
{
    size_t operator()(const ::sw::UnresolvedPackage &p) const
    {
        auto h = std::hash<::sw::PackagePath>()(p.ppath);
        return hash_combine(h, std::hash<::sw::VersionRange>()(p.range));
    }
};

}
