// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "package.h"

#include "database.h"
#include "storage.h"
#include "sw_context.h"

#include <fstream>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "package");

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

static path getHashPathFromHash(const String &h, int nsubdirs, int chars_per_subdir)
{
    path p;
    int i = 0;
    for (; i < nsubdirs; i++)
        p /= h.substr(i * chars_per_subdir, chars_per_subdir);
    p /= h.substr(i * chars_per_subdir);
    return p;
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

UnresolvedPackage &UnresolvedPackage::operator=(const String &s)
{
    return *this = extractFromString(s);
}

String UnresolvedPackage::toString(const String &delim) const
{
    return ppath.toString() + delim + range.toString();
}

bool UnresolvedPackage::canBe(const PackageId &id) const
{
    return ppath == id.ppath && range.hasVersion(id.version);
}

/*ExtendedPackageData UnresolvedPackage::resolve() const
{
    return resolve_dependencies({*this})[*this];
}*/

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

String PackageId::getVariableName() const
{
    auto v = version.toString();
    auto vname = ppath.toString() + "_" + (v == "*" ? "" : ("_" + v));
    std::replace(vname.begin(), vname.end(), '.', '_');
    return vname;
}

/*bool PackageId::canBe(const PackageId &rhs) const
{
return ppath == rhs.ppath
// && version.canBe(rhs.version)
;
}*/

String PackageId::toString(const String &delim) const
{
    return ppath.toString() + delim + version.toString();
}

Package::Package(const Storage &storage, const String &s)
    : storage(storage), PackageId(s)
{
}

Package::Package(const Storage &storage, const PackagePath &p, const Version &v)
    : storage(storage), PackageId(p, v)
{
}

Package::Package(const Storage &storage, const PackageId &id)
    : storage(storage), PackageId(id)
{
}

Package::Package(const Package &rhs)
    : storage(rhs.storage), PackageId(rhs)
{
}

Package &Package::operator=(const Package &rhs)
{
    if (this == &rhs)
        return *this;
    (PackageId &)(*this) = rhs;
    data.reset();
    return *this;
}

Package::~Package() = default;

String Package::getHash() const
{
    // move these calculations to storage?
    switch (storage.getHashSchemaVersion())
    {
    case 1:
        return blake2b_512(ppath.toStringLower() + "-" + version.toString());
    }

    throw SW_RUNTIME_ERROR("Unknown hash schema version: " + std::to_string(storage.getHashSchemaVersion()));
}

path Package::getHashPath() const
{
    // move these calculations to storage?
    switch (storage.getHashPathFromHashSchemaVersion())
    {
    case 1:
        return ::sw::getHashPathFromHash(getHash(), 4, 2); // remote consistent storage paths
    case 2:
        return ::sw::getHashPathFromHash(getHashShort(), 2, 2); // local storage is more relaxed
    }

    throw SW_RUNTIME_ERROR("unreachable");
}

String Package::getHashShort() const
{
    return shorten_hash(getHash(), 8);
}

const PackageData &Package::getData() const
{
    if (!data)
        data = std::make_unique<PackageData>(storage.loadData(*this));
    return *data;
}

/*LocalPackage Package::download() const
{
    return storage.download(*this);
}*/

LocalPackage Package::install() const
{
    return storage.install(*this);
}

LocalPackage::LocalPackage(const LocalStorage &storage, const String &s)
    : Package(storage, s)
{
}

LocalPackage::LocalPackage(const LocalStorage &storage, const PackagePath &p, const Version &v)
    : Package(storage, p, v)
{
}

LocalPackage::LocalPackage(const LocalStorage &storage, const PackageId &id)
    : Package(storage, id)
{
}

const LocalStorage &LocalPackage::getLocalStorage() const
{
    return static_cast<const LocalStorage &>(storage);
}

std::optional<path> LocalPackage::getOverriddenDir() const
{
    return getLocalStorage().getPackagesDatabase().getOverriddenDir(*this);
}

path LocalPackage::getDir() const
{
    return getDir(getLocalStorage().storage_dir_pkg);
}

path LocalPackage::getDir(const path &p) const
{
    return p / getHashPath();
}

path LocalPackage::getDirSrc() const
{
    return getDir() / "src";
}

path LocalPackage::getDirSrc2() const
{
    if (auto d = getOverriddenDir(); d)
        return d.value();
    return getDirSrc() / getSourceDirectoryName();
}

path LocalPackage::getDirObj() const
{
    return getDir() / "obj";
}

path LocalPackage::getDirInfo() const
{
    return getDirSrc() / "info"; // make inf?
}

path LocalPackage::getStampFilename() const
{
    return getDirInfo() / "source.stamp";
}

String LocalPackage::getStampHash() const
{
    String hash;
    std::ifstream ifile(getStampFilename());
    if (ifile)
        ifile >> hash;
    return hash;
}

LocalPackage LocalPackage::getGroupLeader() const
{
    auto id = getLocalStorage().getPackagesDatabase().getGroupLeader(getData().group_number);
    return LocalPackage(storage, id);
}

/*path Package::getDirObjWdir(
// version level, project level (app or project)
) const
{
    return getDir(getStorage().storage_dir_dat) / "wd"; // working directory, was wdir
}*/

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
