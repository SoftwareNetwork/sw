/*
 * SW - Build System and Package Manager
 * Copyright (C) 2016-2019 Egor Pugin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

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
