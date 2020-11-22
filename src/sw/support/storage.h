// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "filesystem.h"
#include "package.h"
#include "package_unresolved.h"
#include "settings.h"

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

struct SW_SUPPORT_API ResolveRequest
{
    UnresolvedPackage u;
    // value or ref?
    PackageSettings settings;
    // value or ref?
    // or take it from swctx?
    // or from sw build - one security ctx for build
    //SecurityContext sctx;

    PackagePtr r;

    bool isResolved() const { return !!r; }

    Package &getPackage() const { return *r; }

    // if package version higher than current, overwrite
    // if both are branches, do not accept new
    // assuming passed package has same package path and branch/version matches
    // input is not null
    void setPackage(PackagePtr);

    bool operator<(const ResolveRequest &rhs) const { return std::tie(u, settings) < std::tie(rhs.u, rhs.settings); }
    bool operator==(const ResolveRequest &rhs) const { return std::tie(u, settings) == std::tie(rhs.u, rhs.settings); }
};

struct SW_SUPPORT_API IResolvableStorage
{
    /// modern resolve call
    virtual bool resolve(ResolveRequest &) const = 0;
};

struct SW_SUPPORT_API IStorage : IResolvableStorage
{
    virtual ~IStorage() = default;

    /// storage schema/settings/capabilities/versions
    virtual const StorageSchema &getSchema() const = 0;

    /// resolve packages from this storage
    //virtual ResolveResult resolve(const UnresolvedPackages &pkgs, UnresolvedPackages &unresolved_pkgs) const = 0;

    /// load package data from this storage
    virtual PackageDataPtr loadData(const PackageId &) const = 0;

    // non virtual methods

    /// resolve packages from this storage with their dependencies
    //ResolveResultWithDependencies resolveWithDependencies(const UnresolvedPackages &pkgs, UnresolvedPackages &unresolved_pkgs) const;
};

struct SW_SUPPORT_API Resolver
{
    virtual ~Resolver() = default;

    virtual bool resolve(ResolveRequest &) const;
    void addStorage(IStorage &);

private:
    std::vector<IStorage *> storages;
};

struct SW_SUPPORT_API CachingResolver : Resolver
{
    CachingResolver(IResolvableStorage &cache);

    bool resolve(ResolveRequest &) const override;

private:
    IResolvableStorage &cache;
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
