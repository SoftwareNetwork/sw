// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2016-2021 Egor Pugin <egor.pugin@gmail.com>

#include "unresolved_package_name.h"

#include "package_name.h"

namespace sw
{

bool contains(const std::unordered_set<UnresolvedPackageName> &upkgs, const PackageName &p)
{
    return std::any_of(upkgs.begin(), upkgs.end(), [&p](auto &u) { return u.contains(p); });
}

UnresolvedPackageName::UnresolvedPackageName(const String &s)
    : UnresolvedPackageName{ extractFromString(s) }
{
}

UnresolvedPackageName::UnresolvedPackageName(const PackagePath &p, const PackageVersionRange &r)
    : ppath(p), range(r)
{
}

UnresolvedPackageName::UnresolvedPackageName(const PackageName &pkg)
    : UnresolvedPackageName(pkg.getPath(), pkg.getVersion())
{
}

UnresolvedPackageName &UnresolvedPackageName::operator=(const String &s)
{
    return *this = extractFromString(s);
}

std::optional<PackageName> UnresolvedPackageName::toPackageName() const
{
    auto v = range.toVersion();
    if (!v)
        return {};
    return PackageName{ ppath, *v };
}

String UnresolvedPackageName::toString(const String &delim) const
{
    auto s = ppath.toString() + delim + range.toString();
    //if (s == delim + "*")
        //s.resize(s.size() - (delim.size() + 1));
    return s;
}

bool UnresolvedPackageName::contains(const PackageName &id) const
{
    return ppath == id.getPath() && range.contains(id.getVersion());
}

UnresolvedPackageName extractFromString(const String &target)
{
    auto [p, v] = split_package_string(target);
    return { p, !v.empty() ? v : PackageVersionRange{} };
}

}
