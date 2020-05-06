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

#pragma once

#include <sw/support/enums.h>
#include <sw/support/package.h>

namespace sw
{

struct IStorage;
struct LocalStorage;
struct OverriddenPackagesStorage;

struct SW_MANAGER_API LocalPackage : Package
{
    LocalPackage(const LocalStorage &, const PackageId &);

    LocalPackage(const LocalPackage &) = default;
    LocalPackage &operator=(const LocalPackage &) = delete;
    LocalPackage(LocalPackage &&) = default;
    LocalPackage &operator=(LocalPackage &&) = default;
    virtual ~LocalPackage() = default;

    virtual std::unique_ptr<Package> clone() const { return std::make_unique<LocalPackage>(*this); }

    bool isOverridden() const;
    std::optional<path> getOverriddenDir() const;

    /// main package dir
    path getDir() const;

    /// source archive root
    path getDirSrc() const;
    /// actual sources root
    path getDirSrc2() const;

    //
    path getDirObj() const;
    path getDirObj(const String &cfg) const;

    //path getDirObjWdir() const;
    path getDirInfo() const;

    path getStampFilename() const;
    String getStampHash() const;

    void remove() const;

    const LocalStorage &getStorage() const;

private:
    path getDir(const path &root) const;
};

using LocalPackagePtr = std::unique_ptr<LocalPackage>;

}

namespace std
{

template<> struct hash<::sw::LocalPackage>
{
    size_t operator()(const ::sw::LocalPackage &p) const
    {
        return std::hash<::sw::Package>()(p);
    }
};

}
