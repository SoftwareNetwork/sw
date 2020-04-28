// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <sw/support/enums.h>
#include <sw/support/package_id.h>

namespace sw
{

struct IStorage;
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

    //
    UnresolvedPackages dependencies;

    // for overridden pkgs
    path sdir;

    //
    // PackageId driver

    virtual ~PackageData() = default;

    virtual std::unique_ptr<PackageData> clone() const { return std::make_unique<PackageData>(*this); }
};

using PackageDataPtr = std::unique_ptr<PackageData>;

struct LocalPackage;
struct OverriddenPackagesStorage;

struct SW_MANAGER_API Package : PackageId
{
    Package(const IStorage &, const PackageId &);

    Package(const Package &);
    Package &operator=(const Package &) = delete;
    Package(Package &&) = default;
    Package &operator=(Package &&) = default;
    virtual ~Package() = default;

    String getHash() const;
    String getHashShort() const;
    path getHashPath() const;

    const PackageData &getData() const;
    const IStorage &getStorage() const;

    virtual std::unique_ptr<Package> clone() const { return std::make_unique<Package>(*this); }

private:
    const IStorage &storage;
    mutable PackageDataPtr data;
};

using PackagePtr = std::unique_ptr<Package>;
//using Packages = std::unordered_set<Package>;

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

    path getDir() const;
    path getDirSrc() const;
    path getDirSrc2() const;
    path getDirObj() const;
    path getDirObj(const String &cfg) const;
    path getDirObjWdir() const;
    path getDirInfo() const;
    path getStampFilename() const;
    String getStampHash() const;

    void remove() const;

private:
    path getDir(const path &root) const;
    const LocalStorage &getLocalStorage() const;
};

using LocalPackagePtr = std::unique_ptr<LocalPackage>;

SW_MANAGER_API
String getSourceDirectoryName();

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
