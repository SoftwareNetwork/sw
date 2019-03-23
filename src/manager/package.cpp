// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "dependency.h"

#include "database.h"
#include "directories.h"
#include "hash.h"
#include "lock.h"
#include "resolver.h"

#include <primitives/sw/cl.h>

#include <boost/algorithm/string.hpp>

#include <fstream>
#include <iostream>
#include <regex>
#include <shared_mutex>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "package");

static cl::opt<bool> separate_bdir("separate-bdir");// , cl::init(true));

namespace sw
{

static std::pair<String, String> split_package_string(const String &s)
{
    /*
    different variants:
        org.sw.demo.package-1.0.0   - the main one currently
        org.sw.demo.package 1.0.0   - very obvious and solid, but not very practical?
        org.sw.demo.package@1.0.0   - not that bad
        org.sw.demo.package/1.0.0   - not that bad, but probably bad rather than good?
    */

    // allow (remove) dashes?
    auto pos = s.find_first_of("-"); // " -@/"
    if (pos == s.npos)
        return { s, {} };
    return { s.substr(0, pos), s.substr(pos + 1) };
}

String getSourceDirectoryName()
{
    // we cannot change it, because server already has such packages
    // introduce versions to change this or smth
    return "sdir";
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
    : UnresolvedPackage(pkg.ppath, pkg.version)
{
}

String UnresolvedPackage::toString(const String &delim) const
{
    return ppath.toString() + delim + range.toString();
}

bool UnresolvedPackage::canBe(const PackageId &id) const
{
    return ppath == id.ppath && range.hasVersion(id.version);
}

ExtendedPackageData UnresolvedPackage::resolve() const
{
    return resolve_dependencies({*this})[*this];

    /*if (auto v = range.getMaxSatisfyingVersion(); v)
        return { ppath, v.value() };
    throw SW_RUNTIME_ERROR("Cannot resolve package: " + toString());*/
}

PackageId::PackageId(const String &target)
{
    auto [p, v] = split_package_string(target);
    ppath = p;
    if (!v.empty())
        version = v;
}

PackageId::PackageId(const PackagePath &p, const Version &v)
    : ppath(p), version(v)
{
}

std::optional<path> PackageId::getOverriddenDir() const
{
    auto &pkgs = getServiceDatabase().getOverriddenPackages();
    auto i = pkgs.find(*this);
    if (i == pkgs.end(*this))
        return {};
    return i->second.sdir;
}

path PackageId::getDir() const
{
    return getDir(getUserDirectories().storage_dir_pkg);
}

path PackageId::getDir(const path &p) const
{
    return p / getHashPath();
}

path PackageId::getDirSrc() const
{
    return getDir(getUserDirectories().storage_dir_pkg) / "src";
}

path PackageId::getDirSrc2() const
{
    if (auto d = getOverriddenDir(); d)
        return d.value();
    return getDirSrc() / getSourceDirectoryName();
}

path PackageId::getDirObj() const
{
    if (!separate_bdir)
        return getDir(getUserDirectories().storage_dir_pkg) / "obj";
    return getDir(getUserDirectories().storage_dir_obj) / "obj";
}

path PackageId::getDirObjWdir(/* version level, project level (app or project) */) const
{
    return getDir(getUserDirectories().storage_dir_dat) / "wd"; // working directory, was wdir
}

path PackageId::getDirInfo() const
{
    return getDirSrc() / "info"; // make inf?
}

path PackageId::getStampFilename() const
{
    return getDirInfo() / "source.stamp";
}

String PackageId::getStampHash() const
{
    String hash;
    std::ifstream ifile(getStampFilename());
    if (ifile)
        ifile >> hash;
    return hash;
}

String PackageId::getHash() const
{
    static const int hash_schema_version = 1;

    switch (hash_schema_version)
    {
    case 1:
        return blake2b_512(ppath.toStringLower() + "-" + version.toString());
    }

    throw SW_RUNTIME_ERROR("unreachable");
}

String PackageId::getFilesystemHash() const
{
    return getHashShort();
}

path PackageId::getHashPath() const
{
    return getHashPathFromHash(getFilesystemHash());
}

static path getHashPathFromHash(const String &h, int nsubdirs, int chars_per_subdir)
{
    path p;
    int i = 0;
    for (; i < nsubdirs; i++)
        p /= h.substr(i * chars_per_subdir, chars_per_subdir);
    p /= h.substr(i * chars_per_subdir);
    return p;
}

path PackageId::getHashPathFull() const
{
    static const int hash_path_from_hash_schema_version = 1;

    switch (hash_path_from_hash_schema_version)
    {
    case 1:
        return ::sw::getHashPathFromHash(getHash(), 4, 2); // remote consistent storage paths
    }

    throw SW_RUNTIME_ERROR("unreachable");
}

#define SHORT_HASH_LEN 8 // was 12 (12 for remote storages)
#define LAST_SUBDIR_LEN 4
#define N_CHARS_PER_SUBDIR 2
#define N_SUBDIRS (SHORT_HASH_LEN - LAST_SUBDIR_LEN) / N_CHARS_PER_SUBDIR

String PackageId::getHashShort() const
{
    return shorten_hash(getHash(), SHORT_HASH_LEN);
}

path PackageId::getHashPathFromHash(const String &h)
{
    return ::sw::getHashPathFromHash(h, N_SUBDIRS, N_CHARS_PER_SUBDIR); // local storage is more relaxed
}

String PackageId::getVariableName() const
{
    if (variable_name.empty())
    {
        auto v = version.toString();
        auto vname = ppath.toString() + "_" + (v == "*" ? "" : ("_" + v));
        std::replace(vname.begin(), vname.end(), '.', '_');
        return vname;
    }
    return variable_name;
}

bool PackageId::canBe(const PackageId &rhs) const
{
    return ppath == rhs.ppath/* && version.canBe(rhs.version)*/;
}

Package PackageId::toPackage() const
{
    Package p;
    p.ppath = ppath;
    p.version = version;
    return p;
}

String PackageId::toString(const String &delim) const
{
    return ppath.toString() + delim + version.toString();
}

PackageId extractFromStringPackageId(const String &target)
{
    PackageId p;
    auto [pp, v] = split_package_string(target);
    if (v.empty())
        throw SW_RUNTIME_ERROR("Bad target: " + target);
    p.ppath = pp;
    p.version = v;
    return p;
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
