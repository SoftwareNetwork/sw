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

String getSourceDirectoryName()
{
    // we cannot change it, because server already has such packages
    // introduce versions to change this or smth
    return "sdir";
}

LocalPackage::LocalPackage(const LocalStorage &storage, const PackageId &id)
    : Package(storage, id)
{
}

const LocalStorage &LocalPackage::getStorage() const
{
    return static_cast<const LocalStorage &>(Package::getStorage());
}

bool LocalPackage::isOverridden() const
{
    return getStorage().isPackageOverridden(*this);
}

std::optional<path> LocalPackage::getOverriddenDir() const
{
    if (isOverridden() && !getData().sdir.empty())
        return getData().sdir;
    return {};
}

path LocalPackage::getDir() const
{
    return getDir(getStorage().storage_dir_pkg);
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

path LocalPackage::getDirObj(const String &cfg) const
{
    // bld was build
    return getDirObj() / "bld" / cfg;
}

path LocalPackage::getDirInfo() const
{
    // maybe you getDir()? because gitDirSrc() is unpacked from archive
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

/*path LocalPackage::getDirObjWdir(
// version level, project level (app or project)
) const
{
    //return getDir(getStorage().storage_dir_dat) / "wd"; // working directory, was wdir
    return getDir() / "wd"; // working directory, was wdir
}*/

void LocalPackage::remove() const
{
    getStorage().remove(*this);
}

}
