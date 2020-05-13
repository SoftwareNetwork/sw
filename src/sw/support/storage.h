// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "package.h"

#include "filesystem.h"

namespace sw
{

struct PackageId;
struct PackageData;

struct SW_SUPPORT_API Directories
{
    path storage_dir;
#define DIR(x) path storage_dir_##x;
#include "storage_directories.inl"
#undef DIR

    Directories(const path &root);

    path getDatabaseRootDir() const;
};

struct StorageSchema
{
    StorageSchema(int hash_version, int hash_path_version)
        : hash_version(hash_version), hash_path_version(hash_path_version)
    {}

    int getHashVersion() const { return hash_version; }
    int getHashPathFromHashVersion() const { return hash_path_version; }

private:
    int hash_version;
    int hash_path_version;
};

struct SoftwareNetworkStorageSchema : StorageSchema
{
    SoftwareNetworkStorageSchema() : StorageSchema(1, 1) {}
};

struct SW_SUPPORT_API IStorage
{
    virtual ~IStorage() = default;

    /// storage schema/settings/capabilities/versions
    virtual const StorageSchema &getSchema() const = 0;

    /// resolve packages from this storage
    virtual std::unordered_map<UnresolvedPackage, PackagePtr> resolve(const UnresolvedPackages &pkgs, UnresolvedPackages &unresolved_pkgs) const = 0;

    /// load package data from this storage
    virtual PackageDataPtr loadData(const PackageId &) const = 0;

    // non virtual methods

    /// resolve packages from this storage with their dependencies
    std::unordered_map<UnresolvedPackage, PackagePtr> resolveWithDependencies(const UnresolvedPackages &pkgs, UnresolvedPackages &unresolved_pkgs) const;
};

SW_SUPPORT_API
int readPackagesDatabaseVersion(const path &dir);

SW_SUPPORT_API
String getPackagesDatabaseVersionFileName();

SW_SUPPORT_API
int getPackagesDatabaseSchemaVersion();

SW_SUPPORT_API
String getPackagesDatabaseSchemaVersionFileName();

} // namespace sw
