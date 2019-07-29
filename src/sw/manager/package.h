// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "enums.h"
#include "package_id.h"

namespace sw
{

struct Storage;
struct LocalStorage;

struct PackageData
{
    // internal id?
    //db::PackageVersionId id = 0;

    SomeFlags flags;

    // package hash (sw.tar.gz)
    String hash;

    // length of prefix path
    // e.g., if package path is 'myproject.pkg' and it's added to 'org.sw',
    // then prefix equals to size of 'org.sw', thus 2
    int prefix = 2;

    // packages added in one bunch (=group) have the same group number
    PackageVersionGroupNumber group_number = 0;

    //
    UnresolvedPackages dependencies;

    // for overridden pkgs
    path sdir;
};

struct LocalPackage;
struct OverriddenPackagesStorage;

struct SW_MANAGER_API Package : PackageId
{
    const Storage &storage;

    Package(const Storage &, const String &);
    Package(const Storage &, const PackagePath &, const Version &);
    Package(const Storage &, const PackageId &);
    Package(const Package &) = default;
    Package &operator=(const Package &) = default;
    Package(Package &&) = default;
    Package &operator=(Package &&) = default;
    ~Package() = default;

    String getHash() const;
    String getHashShort() const;
    path getHashPath() const;

    //void setData(const PackageData &) const;
    const PackageData &getData() const;
    //LocalPackage download(file type) const;
    //LocalPackage install() const;
};

struct SW_MANAGER_API LocalPackage : Package
{
    LocalPackage(const LocalStorage &, const String &);
    LocalPackage(const LocalStorage &, const PackagePath &, const Version &);
    LocalPackage(const LocalStorage &, const PackageId &);

    LocalPackage(const LocalPackage &) = default;
    LocalPackage &operator=(const LocalPackage &) = default;
    LocalPackage(LocalPackage &&) = default;
    LocalPackage &operator=(LocalPackage &&) = default;
    ~LocalPackage() = default;

    bool isOverridden() const;
    std::optional<path> getOverriddenDir() const;

    path getDir() const;
    path getDirSrc() const;
    path getDirSrc2() const;
    path getDirObj() const;
    path getDirObjWdir() const;
    path getDirInfo() const;
    path getStampFilename() const;
    String getStampHash() const;

private:
    path getDir(const path &root) const;
    const LocalStorage &getLocalStorage() const;
};

using Packages = std::unordered_set<Package>;

}

namespace std
{

template<> struct hash<::sw::Package>
{
    size_t operator()(const ::sw::Package &p) const
    {
        return std::hash<::sw::PackageId>()(p);
    }
};

template<> struct hash<::sw::LocalPackage>
{
    size_t operator()(const ::sw::LocalPackage &p) const
    {
        return std::hash<::sw::Package>()(p);
    }
};

}
