// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "package_data.h"

namespace sw
{

PackageId PackageData::getPackageId(const PackagePath &prefix) const
{
    PackageId id;
    id.ppath = prefix / ppath;
    id.version = version;
    return id;
}

void PackageData::applyPrefix(const PackagePath &prefix)
{
    ppath = prefix / ppath;

    // also fix deps
    decltype(dependencies) deps2;
    for (auto &[p, r] : dependencies)
    {
        if (p.isAbsolute())
            deps2.insert(UnresolvedPackage{p,r});
        else
            deps2.insert(UnresolvedPackage{ prefix / p,r });
    }
    dependencies = deps2;
}

void PackageData::checkSourceAndVersion()
{
    ::sw::checkSourceAndVersion(source, version);
}

void checkSourceAndVersion(Source &s, const Version &v)
{
    applyVersionToUrl(s, v);

    if (!isValidSourceUrl(s))
        throw std::runtime_error("Invalid source: " + print_source(s));
}

}
