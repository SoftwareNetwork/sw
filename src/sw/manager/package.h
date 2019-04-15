// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "enums.h"
#include "package_unresolved.h"

namespace sw
{

struct Storage;
struct LocalStorage;

struct SW_MANAGER_API PackageId
{
    PackagePath ppath;
    Version version;

    //PackageId() = default;
    // try to extract from string
    PackageId(const String &);
    PackageId(const PackagePath &, const Version &);

    PackagePath getPath() const { return ppath; }
    Version getVersion() const { return version; }

    //bool canBe(const PackageId &rhs) const;
    //bool empty() const { return ppath.empty(); }

    bool isPublic() const { return ppath.isPublic(); }
    bool isPrivate() const { return ppath.isPrivate(); }

    bool isUser() const { return ppath.isUser(); }
    bool isOrganization() const { return ppath.isOrganization(); }

    bool operator<(const PackageId &rhs) const { return std::tie(ppath, version) < std::tie(rhs.ppath, rhs.version); }
    bool operator==(const PackageId &rhs) const { return std::tie(ppath, version) == std::tie(rhs.ppath, rhs.version); }
    bool operator!=(const PackageId &rhs) const { return !operator==(rhs); }

    String getVariableName() const;

    String toString(const String &delim = "-") const;
};

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
    int prefix;

    // packages added in one bunch (=group) have the same group number
    PackageVersionGroupNumber group_number;

    //
    UnresolvedPackages dependencies;
};

struct SW_MANAGER_API Package : PackageId
{
    const Storage &storage;

    Package(const Storage &storage, const String &);
    Package(const Storage &storage, const PackagePath &, const Version &);
    Package(const Storage &storage, const PackageId &id);
    Package(const Package &);
    Package &operator=(const Package &);
    Package(Package &&) = default;
    Package &operator=(Package &&) = default;
    ~Package();

    String getHash() const;
    String getHashShort() const;
    path getHashPath() const;

    const PackageData &getData() const;
    //struct LocalPackage download() const;
    struct LocalPackage install() const;

private:
    mutable std::unique_ptr<PackageData> data;
};

struct SW_MANAGER_API LocalPackage : Package
{
    using Package::Package;

    LocalPackage(const LocalStorage &storage, const String &);
    LocalPackage(const LocalStorage &storage, const PackagePath &, const Version &);
    LocalPackage(const LocalStorage &storage, const PackageId &id);

    std::optional<path> getOverriddenDir() const;

    path getDir() const;
    path getDirSrc() const;
    path getDirSrc2() const;
    path getDirObj() const;
    //path getDirObjWdir() const;
    path getDirInfo() const;
    path getStampFilename() const;
    String getStampHash() const;

    LocalPackage getGroupLeader() const;

private:
    path getDir(const path &p) const;
    const LocalStorage &getLocalStorage() const;
};

using PackageIdSet = std::unordered_set<PackageId>;
using Packages = std::unordered_set<Package>;

struct SW_MANAGER_API PackageDescriptionInternal
{
    virtual ~PackageDescriptionInternal() = default;
    virtual std::tuple<path /* root dir */, Files> getFiles() const = 0;
    virtual UnresolvedPackages getDependencies() const = 0;

    // source
    // icons
    // screenshots, previews
    // desc: type, summary,
};

SW_MANAGER_API
UnresolvedPackage extractFromString(const String &target);

SW_MANAGER_API
PackageId extractPackageIdFromString(const String &target);

SW_MANAGER_API
String getSourceDirectoryName();

}

namespace std
{

template<> struct hash<::sw::PackageId>
{
    size_t operator()(const ::sw::PackageId &p) const
    {
        auto h = std::hash<::sw::PackagePath>()(p.ppath);
        return hash_combine(h, std::hash<::sw::Version>()(p.version));
    }
};

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
