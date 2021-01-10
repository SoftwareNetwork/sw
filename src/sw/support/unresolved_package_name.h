// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "package_path.h"
#include "version.h"

#include <unordered_set>

namespace sw
{

struct PackageName;

struct SW_SUPPORT_API UnresolvedPackageName
{
    UnresolvedPackageName(const String &s);
    UnresolvedPackageName(const PackagePath &p, const PackageVersionRange &r);
    UnresolvedPackageName(const PackageName &);

    UnresolvedPackageName &operator=(const String &s);

    const PackagePath &getPath() const { return ppath; }
    const PackageVersionRange &getRange() const { return range; }

    std::optional<PackageName> toPackageName() const;
    String toString(const String &delim = "-") const;
    [[deprecated("use contains()")]]
    bool canBe(const PackageName &id) const { return contains(id); }
    bool contains(const PackageName &) const;

    //bool operator<(const UnresolvedPackage &rhs) const { return std::tie(ppath, range) < std::tie(rhs.ppath, rhs.range); }
    bool operator==(const UnresolvedPackageName &rhs) const { return std::tie(ppath, range) == std::tie(rhs.ppath, rhs.range); }
    //bool operator!=(const UnresolvedPackage &rhs) const { return !operator==(rhs); }

private:
    PackagePath ppath;
    PackageVersionRange range;
};

//using UnresolvedPackages = std::unordered_set<UnresolvedPackageName>;

SW_SUPPORT_API
bool contains(const std::unordered_set<UnresolvedPackageName> &, const PackageName &);

SW_SUPPORT_API
UnresolvedPackageName extractFromString(const String &target);

}

namespace std
{

template<> struct hash<::sw::UnresolvedPackageName>
{
    size_t operator()(const ::sw::UnresolvedPackageName &p) const
    {
        auto h = std::hash<::sw::PackagePath>()(p.getPath());
        return hash_combine(h, std::hash<::sw::PackageVersionRange>()(p.getRange()));
    }
};

}
