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

struct SecurityContext
{
    // TODO:
    bool check() { return true; }
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
};

struct SW_SUPPORT_API ResolveResultWithDependencies
{
    ResolveResult m;
    std::unordered_map<UnresolvedPackage, size_t> h;

    ResolveResultWithDependencies() = default;
    ResolveResultWithDependencies(ResolveResult &&in_m) { m = std::move(in_m); }
    ResolveResultWithDependencies(const ResolveResultWithDependencies &) = delete;
    ResolveResultWithDependencies &operator=(const ResolveResultWithDependencies &) = delete;
    ResolveResultWithDependencies(ResolveResultWithDependencies &&) = default;
    ResolveResultWithDependencies &operator=(ResolveResultWithDependencies &&) = default;

    bool empty() const { return m.empty(); }

    auto begin() { return m.begin(); }
    auto end() { return m.end(); }

    auto begin() const { return m.begin(); }
    auto end() const { return m.end(); }

    auto find(const UnresolvedPackage &u) { return m.find(u); }
    auto find(const UnresolvedPackage &u) const { return m.find(u); }

    auto &operator[](const UnresolvedPackage &u) { return m[u]; }

    void merge(ResolveResultWithDependencies &m2)
    {
        m.merge(m2.m);
        h.merge(m2.h);
    }

    Package &get(const UnresolvedPackage &u)
    {
        auto i = find(u);
        if (i == end())
            throw SW_RUNTIME_ERROR("No such unresolved package: " + u.toString());
        return *i->second;
    }

    const Package &get(const UnresolvedPackage &u) const
    {
        auto i = find(u);
        if (i == end())
            throw SW_RUNTIME_ERROR("No such unresolved package: " + u.toString());
        return *i->second;
    }

    size_t getHash(const UnresolvedPackage &u);
};

struct SW_SUPPORT_API IStorage
{
    virtual ~IStorage() = default;

    /// storage schema/settings/capabilities/versions
    virtual const StorageSchema &getSchema() const = 0;

    /// resolve packages from this storage
    //virtual ResolveResult resolve(const UnresolvedPackages &pkgs, UnresolvedPackages &unresolved_pkgs) const = 0;

    /// modern resolve call
    virtual void resolve(ResolveRequest &) const = 0;

    /// load package data from this storage
    virtual PackageDataPtr loadData(const PackageId &) const = 0;

    // non virtual methods

    /// resolve packages from this storage with their dependencies
    ResolveResultWithDependencies resolveWithDependencies(const UnresolvedPackages &pkgs, UnresolvedPackages &unresolved_pkgs) const;
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
