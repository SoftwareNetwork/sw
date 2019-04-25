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

/*void Package::setData(const PackageData &d) const
{
    data = std::make_unique<PackageData>(d);
}*/

const PackageData &Package::getData() const
{
    return storage.loadData(*this);
}

/*LocalPackage Package::download() const
{
    return storage.download(*this);
}*/

/*LocalPackage Package::install() const
{
    return storage.install(*this);
}*/

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

bool LocalPackage::isOverridden() const
{
    return getLocalStorage().isPackageOverridden(*this);
}

std::optional<path> LocalPackage::getOverriddenDir() const
{
    if (isOverridden() && !getData().sdir.empty())
        return getData().sdir;
    return {};
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
    return LocalPackage(getLocalStorage(), id);
}

path LocalPackage::getDirObjWdir(
// version level, project level (app or project)
) const
{
    //return getDir(getStorage().storage_dir_dat) / "wd"; // working directory, was wdir
    return getDir() / "wd"; // working directory, was wdir
}

}
