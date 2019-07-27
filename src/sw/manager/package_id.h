// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "enums.h"
#include "package_unresolved.h"

namespace sw
{

struct SW_MANAGER_API PackageId
{
    PackagePath ppath;
    Version version;

    // try to extract from string
    PackageId(const String &);
    PackageId(const PackagePath &, const Version &);

    PackagePath getPath() const { return ppath; }
    Version getVersion() const { return version; }

    bool isPublic() const { return ppath.isPublic(); }
    bool isPrivate() const { return ppath.isPrivate(); }

    bool isUser() const { return ppath.isUser(); }
    bool isOrganization() const { return ppath.isOrganization(); }

    bool operator<(const PackageId &rhs) const { return std::tie(ppath, version) < std::tie(rhs.ppath, rhs.version); }
    bool operator==(const PackageId &rhs) const { return std::tie(ppath, version) == std::tie(rhs.ppath, rhs.version); }
    bool operator!=(const PackageId &rhs) const { return !operator==(rhs); }

    String getVariableName() const;

    String toString(const String &delim = "-") const;
};

using PackageIdSet = std::unordered_set<PackageId>;

SW_MANAGER_API
PackageId extractPackageIdFromString(const String &target);

SW_MANAGER_API
String getSourceDirectoryName();

}

namespace std
{

template<> struct hash<::sw::PackageId>
{
    size_t operator()(const ::sw::PackageId &p) const
    {
        auto h = std::hash<::sw::PackagePath>()(p.ppath);
        return hash_combine(h, std::hash<::sw::Version>()(p.version));
    }
};

}
