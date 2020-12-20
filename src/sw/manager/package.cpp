// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>

#include "package.h"

#include "database.h"
#include "storage.h"
#include "sw_context.h"

#include <fstream>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "package");

namespace sw
{

LocalPackage::LocalPackage(const LocalStorage &storage, const PackageId &id)
    : Package(storage, id)
{
}

const LocalStorage &LocalPackage::getStorage() const
{
    return static_cast<const LocalStorage &>(Package::getStorage());
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
    SW_UNIMPLEMENTED;
    //getStorage().remove(*this);
}

path OverriddenPackage::getDirSrc2() const
{
    return getData().sdir;
}

}
