// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "package.h"

#include <sw/support/filesystem.h>

#include <primitives/date_time.h>

namespace sw
{

struct LocalPackage;
struct PackageId;
struct PackageData;

namespace vfs
{

/*struct SW_MANAGER_API VirtualFileSystem
{
    virtual ~VirtualFileSystem() = default;

    virtual void writeFile(const PackageId &pkg, const path &local_file, const path &vfs_file) const = 0;
};

// default fs
struct SW_MANAGER_API LocalFileSystem : VirtualFileSystem
{
};

// more than one destination
struct SW_MANAGER_API VirtualFileSystemMultiplexer : VirtualFileSystem
{
    std::vector<std::shared_ptr<VirtualFileSystem>> filesystems;

    void writeFile(const PackageId &pkg, const path &local_file, const path &vfs_file) const override
    {
        for (auto &fs : filesystems)
            fs->writeFile(pkg, local_file, vfs_file);
    }
};*/

struct SW_MANAGER_API File
{
    virtual ~File() = default;

    virtual bool copy(const path &to) const = 0;
};

} // namespace vfs

struct SW_MANAGER_API Directories
{
    path storage_dir;
#define DIR(x) path storage_dir_##x;
#include "storage_directories.inl"
#undef DIR

    Directories(const path &root);
};

enum class StorageFileType
{
    // or Archive or DataArchive
    SourceArchive       =   0x1,

    // db?
    // binary files
    // dbg info
    // data files
    // config files
    // used files
};

SW_MANAGER_API
String toUserString(StorageFileType);

struct PackagesDatabase;
struct ServiceDatabase;
struct SwContext;

struct IStorage
{
    virtual ~IStorage() = default;

    virtual String getName() const = 0;

    // storage schema/settings/capabilities/versions
    virtual int getHashSchemaVersion() const = 0;
    virtual int getHashPathFromHashSchemaVersion() const = 0;

    //
    virtual PackageData loadData(const PackageId &) const = 0;
    //virtual LocalPackage download(const PackageId &) const = 0;
    virtual LocalPackage install(const Package &) const = 0;
    virtual std::unordered_map<UnresolvedPackage, Package> resolve(const UnresolvedPackages &pkgs, UnresolvedPackages &unresolved_pkgs) const = 0;

    // data exchange

    // get predefined file
    virtual void get(const IStorage &source, const PackageId &id, StorageFileType) = 0;

    /// get specific file from storage from package directory
    //virtual void get(const IStorage &source, const PackageId &id, const path &from_rel_path, const path &to_file) = 0;

    // return array of multiple re-try paths?
    virtual std::unique_ptr<vfs::File> getFile(const PackageId &id, StorageFileType) const = 0;
};

struct SW_MANAGER_API Storage : IStorage
{
    Storage(const String &name);

    String getName() const override { return name; }

private:
    String name;
};

struct SW_MANAGER_API StorageWithPackagesDatabase : Storage
{
    StorageWithPackagesDatabase(const String &name, const path &db_dir);
    virtual ~StorageWithPackagesDatabase();

    PackageData loadData(const PackageId &) const override;
    void get(const IStorage &source, const PackageId &id, StorageFileType) override;
    std::unordered_map<UnresolvedPackage, Package> resolve(const UnresolvedPackages &pkgs, UnresolvedPackages &unresolved_pkgs) const override;

//protected:?
    PackagesDatabase &getPackagesDatabase() const;

private:
    std::unique_ptr<PackagesDatabase> pkgdb;

    std::unordered_map<UnresolvedPackage, Package> resolve_no_deps(const UnresolvedPackages &pkgs, UnresolvedPackages &unresolved_pkgs) const;
};

struct SW_MANAGER_API LocalStorageBase : StorageWithPackagesDatabase
{
    LocalStorageBase(const String &name, const path &db_dir);
    virtual ~LocalStorageBase();

    int getHashSchemaVersion() const override;
    int getHashPathFromHashSchemaVersion() const override;

    std::unique_ptr<vfs::File> getFile(const PackageId &id, StorageFileType) const override;

    void deletePackage(const PackageId &id) const;
};

struct SW_MANAGER_API OverriddenPackagesStorage : LocalStorageBase
{
    const LocalStorage &ls;

    OverriddenPackagesStorage(const LocalStorage &ls, const path &db_dir);
    virtual ~OverriddenPackagesStorage();

    LocalPackage install(const Package &) const override;

    std::unordered_set<LocalPackage> getPackages() const;
    void deletePackageDir(const path &sdir) const;
};

struct SW_MANAGER_API LocalStorage : Directories, LocalStorageBase
{
    LocalStorage(const path &local_storage_root_dir);
    virtual ~LocalStorage();

    path getDatabaseRootDir() const;

    //LocalPackage download(const PackageId &) const override;
    LocalPackage install(const Package &) const override;
    void get(const IStorage &source, const PackageId &id, StorageFileType) override;
    bool isPackageInstalled(const Package &id) const;
    std::unordered_map<UnresolvedPackage, Package> resolve(const UnresolvedPackages &pkgs, UnresolvedPackages &unresolved_pkgs) const override;

    OverriddenPackagesStorage &getOverriddenPackagesStorage();
    const OverriddenPackagesStorage &getOverriddenPackagesStorage() const;

private:
    OverriddenPackagesStorage ovs;

    void migrateStorage(int from, int to);
};

// main/web/url etc. storage
struct SW_MANAGER_API RemoteStorage : StorageWithPackagesDatabase
{
    // also pass url, etc.
    // maybe pass root_db_dir / name directly
    RemoteStorage(LocalStorage &ls, const String &name, const path &root_db_dir);
    virtual ~RemoteStorage();

    int getHashSchemaVersion() const override;
    int getHashPathFromHashSchemaVersion() const override;
    //LocalPackage download(const PackageId &) const override;
    LocalPackage install(const Package &) const override;
    std::unordered_map<UnresolvedPackage, Package> resolve(const UnresolvedPackages &pkgs, UnresolvedPackages &unresolved_pkgs) const override;
    std::unique_ptr<vfs::File> getFile(const PackageId &id, StorageFileType) const override;

private:
    LocalStorage &ls;
    path db_repo_dir;

    void download() const;
    void load() const;
    void updateDb() const;
    void preInitFindDependencies() const;
    void writeDownloadTime() const;
    TimePoint readDownloadTime() const;
    bool isCurrentDbOld() const;
};

} // namespace sw
