// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "enums.h"
#include "package_unresolved.h"

namespace sw
{

struct SW_SUPPORT_API PackageId
{
    // try to extract from string
    PackageId(const String &);
    PackageId(const PackagePath &, const PackageVersion &);

    const PackagePath &getPath() const { return ppath; }
    const PackageVersion &getVersion() const { return version; }

    bool operator<(const PackageId &rhs) const { return std::tie(ppath, version) < std::tie(rhs.ppath, rhs.version); }
    bool operator==(const PackageId &rhs) const { return std::tie(ppath, version) == std::tie(rhs.ppath, rhs.version); }

    String getVariableName() const;

    [[nodiscard]]
    String toString(const String &delim = "-") const;
    // toPackageRangeString()?
    [[nodiscard]]
    std::string toRangeString(const String &delim = "-") const;

private:
    PackagePath ppath;
    PackageVersion version;
};

using PackageIdSet = std::unordered_set<PackageId>;

SW_SUPPORT_API
PackageId extractPackageIdFromString(const String &target);

std::pair<String, String> split_package_string(const String &s);

}

namespace std
{

template<> struct hash<::sw::PackageId>
{
    size_t operator()(const ::sw::PackageId &p) const
    {
        auto h = std::hash<::sw::PackagePath>()(p.getPath());
        return hash_combine(h, std::hash<::sw::PackageVersion>()(p.getVersion()));
    }
};

}
