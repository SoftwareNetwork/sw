// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "package_path.h"
#include "version.h"

namespace sw
{

struct SW_SUPPORT_API PackageName
{
    // try to extract from string
    PackageName(const String &);
    PackageName(const PackagePath &, const PackageVersion &);

    const PackagePath &getPath() const { return ppath; }
    const PackageVersion &getVersion() const { return version; }

    bool operator<(const PackageName &rhs) const { return std::tie(ppath, version) < std::tie(rhs.ppath, rhs.version); }
    bool operator==(const PackageName &rhs) const { return std::tie(ppath, version) == std::tie(rhs.ppath, rhs.version); }

    //String getVariableName() const;

    // remove delim? users can invoke getpath().tostring() + "whatever they want" + getversion().tostring()
    [[nodiscard]]
    String toString(const String &delim = "-") const;
    // toPackageRangeString()?
    [[nodiscard]]
    std::string toRangeString(const String &delim = "-") const;

private:
    PackagePath ppath;
    PackageVersion version;
};

//using PackageNameUSet = std::unordered_set<PackageName>;

SW_SUPPORT_API
[[nodiscard]]
PackageName extractPackageIdFromString(const String &target);

[[nodiscard]]
std::pair<String, String> split_package_string(const String &s);

}

namespace std
{

template<> struct hash<::sw::PackageName>
{
    size_t operator()(const ::sw::PackageName &p) const
    {
        auto h = std::hash<::sw::PackagePath>()(p.getPath());
        return hash_combine(h, std::hash<::sw::PackageVersion>()(p.getVersion()));
    }
};

}
