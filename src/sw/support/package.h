// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "enums.h"
#include "package_id.h"
#include "settings.h"

namespace sw
{

struct IStorage;

struct SW_SUPPORT_API PackageData
{
    // internal id?
    //db::PackageVersionId id = 0;

    SomeFlags flags;

    // source package hash (sw.tar.gz)
    String hash;
    String source;

    // length of prefix path
    // e.g., if package path is 'myproject.pkg' and it's added to 'org.sw',
    // then prefix equals to size of 'org.sw', thus 2
    int prefix = 2;

    //
    UnresolvedPackages dependencies;

    // for overridden pkgs
    path sdir;

    // settings
    // move to other place?
    PackageSettings settings;

    //
    PackageId driver;

    PackageData(const PackageId &driver_id);
    virtual ~PackageData() = default;

    virtual std::unique_ptr<PackageData> clone() const { return std::make_unique<PackageData>(*this); }

    virtual String getHash(/*StorageFileType type, */size_t config_hash = 0) const;
};

using PackageDataPtr = std::unique_ptr<PackageData>;

struct SW_SUPPORT_API Package : PackageId
{
    Package(const IStorage &, const PackageId &);

    Package(const Package &);
    Package &operator=(const Package &) = delete;
    Package(Package &&) = default;
    Package &operator=(Package &&) = delete;
    virtual ~Package() = default;

    String getHash() const;
    String getHashShort() const;
    path getHashPath() const;

    String formatPath(const String &) const;

    const PackageData &getData() const;
    const IStorage &getStorage() const;

    virtual std::unique_ptr<Package> clone() const { return std::make_unique<Package>(*this); }

    virtual bool isInstallable() const { return true; }

private:
    const IStorage &storage;
    mutable PackageDataPtr data;
};

using PackagePtr = std::unique_ptr<Package>;
//using Packages = std::unordered_set<Package>;
//using ResolveResult = std::unordered_map<UnresolvedPackage, PackagePtr>;

SW_SUPPORT_API
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

}
