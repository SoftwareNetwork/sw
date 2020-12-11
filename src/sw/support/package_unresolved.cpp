// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>

#include "package_id.h"

//#include <primitives/log.h>
//DECLARE_STATIC_LOGGER(logger, "package_id");

namespace sw
{

bool contains(const UnresolvedPackages &upkgs, const PackageId &p)
{
    return std::any_of(upkgs.begin(), upkgs.end(), [&p](auto &u) { return u.contains(p); });
}

UnresolvedPackage::UnresolvedPackage(const String &s)
{
    *this = extractFromString(s);
}

UnresolvedPackage::UnresolvedPackage(const PackagePath &p, const PackageVersionRange &r)
{
    ppath = p;
    range = r;
}

UnresolvedPackage::UnresolvedPackage(const PackageId &pkg)
    : UnresolvedPackage(pkg.getPath(), pkg.getVersion())
{
}

UnresolvedPackage &UnresolvedPackage::operator=(const String &s)
{
    return *this = extractFromString(s);
}

std::optional<PackageId> UnresolvedPackage::toPackageId() const
{
    auto v = range.toVersion();
    if (!v)
        return {};
    return PackageId{ ppath, *v };
}

String UnresolvedPackage::toString(const String &delim) const
{
    auto s = ppath.toString() + delim + range.toString();
    //if (s == delim + "*")
        //s.resize(s.size() - (delim.size() + 1));
    return s;
}

bool UnresolvedPackage::contains(const PackageId &id) const
{
    return ppath == id.getPath() && range.contains(id.getVersion());
}

UnresolvedPackage extractFromString(const String &target)
{
    auto [p, v] = split_package_string(target);
    return { p, !v.empty() ? v : PackageVersionRange{} };
}

}
