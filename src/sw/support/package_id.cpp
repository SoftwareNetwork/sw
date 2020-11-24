// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>

#include "package_id.h"

//#include <primitives/log.h>
//DECLARE_STATIC_LOGGER(logger, "package_id");

namespace sw
{

static std::pair<String, String> split_package_string(const String &s)
{
    /*
    different variants:
        org.sw.demo.package-1.0.0   - the main one currently, but it's hard to use '-' in ppath
        org.sw.demo.package 1.0.0   - very obvious and solid, but not very practical?
        org.sw.demo.package@1.0.0   - not that bad
        org.sw.demo.package/1.0.0   - not that bad, but probably bad rather than good?

    other cases (?):
        org.sw.demo.package-with-dashes--1.0.0   - double dash to indicate halfs (@ and ' ' also work)
    */

    size_t pos;

    // fancy case
    /*pos = s.find_first_of("@/"); // space (' ') can be met in version, so we'll fail in this case
    if (pos != s.npos)
        return { s.substr(0, pos), s.substr(pos + 1) };

    // double dashed case
    pos = s.find("--");
    if (pos != s.npos)
        return { s.substr(0, pos), s.substr(pos + 1) };*/

    // simple dash + space case
    pos = s.find_first_of("-"); // also space ' '?
    if (pos == s.npos)
        return { s, {} };
    return { s.substr(0, pos), s.substr(pos + 1) };
}

UnresolvedPackage::UnresolvedPackage(const String &s)
{
    *this = extractFromString(s);
}

UnresolvedPackage::UnresolvedPackage(const PackagePath &p, const VersionRange &r)
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

bool contains(const UnresolvedPackages &upkgs, const PackageId &p)
{
    return std::any_of(upkgs.begin(), upkgs.end(), [&p](auto &u) { return u.contains(p); });
}

PackageId::PackageId(const String &target)
{
    auto [p, v] = split_package_string(target);
    ppath = p;
    if (v.empty())
        throw SW_RUNTIME_ERROR("Empty version when constructing package id '" + target + "', resolve first");
    version = v;
}

PackageId::PackageId(const PackagePath &p, const Version &v)
    : ppath(p), version(v)
{
}

String PackageId::getVariableName() const
{
    auto v = version.toString();
    auto vname = ppath.toString() + "_" + (v == "*" ? "" : ("_" + v));
    std::replace(vname.begin(), vname.end(), '.', '_');
    return vname;
}

String PackageId::toString() const
{
    return toString("-");
}

String PackageId::toString(const String &delim) const
{
    return ppath.toString() + delim + version.toString();
}

String PackageId::toString(Version::Level level, const String &delim) const
{
    return ppath.toString() + delim + version.toString(level);
}

PackageId extractPackageIdFromString(const String &target)
{
    auto [pp, v] = split_package_string(target);
    if (v.empty())
        throw SW_RUNTIME_ERROR("Bad target: " + target);
    return {pp, v};
}

UnresolvedPackage extractFromString(const String &target)
{
    UnresolvedPackage u;
    auto [p, v] = split_package_string(target);
    u.ppath = p;
    if (!v.empty())
        u.range = v;
    return u;
}

}
