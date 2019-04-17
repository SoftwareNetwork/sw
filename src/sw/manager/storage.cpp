// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "storage.h"

#include "database.h"

#include <primitives/pack.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "storage");

static const String packages_db_name = "packages.db";

namespace sw
{

/*namespace detail
{

struct VirtualFileSystem
{
    virtual ~VirtualFileSystem() = default;

    virtual void writeFile(const PackageId &pkg, const path &local_file, const path &vfs_file) const = 0;
};

// default fs
struct LocalFileSystem : VirtualFileSystem
{

};

// more than one destination
struct VirtualFileSystemMultiplexer : VirtualFileSystem
{
    std::vector<std::shared_ptr<VirtualFileSystem>> filesystems;

    void writeFile(const PackageId &pkg, const path &local_file, const path &vfs_file) const override
    {
        for (auto &fs : filesystems)
            fs->writeFile(pkg, local_file, vfs_file);
    }
};

struct StorageFileVerifier
{
    // can be hash
    // can be digital signature
};

struct IStorage
{
    virtual ~IStorage() = default;

    virtual path getRelativePath(const path &p) const = 0;

    // bool isDirectory()?

    // when available?
    // commit()
    // rollback()
};

struct Storage : IStorage
{
    // make IStorage?
    std::shared_ptr<VirtualFileSystem> vfs;

    Storage(const std::shared_ptr<VirtualFileSystem> &vfs)
        : vfs(vfs)
    {
    }

    virtual ~Storage() = default;

    path getRelativePath(const path &p) const override
    {
        if (p.is_relative())
            return p;
        return p;
    }

    // from remote

    // low level
    void copyFile(const Storage &remote_storage, const path &p);
    // fine
    void copyFile(const Storage &remote_storage, const PackageId &pkg, const path &p);
    // less generic
    void copyFile(const Storage &remote_storage, const PackageId &pkg, const path &p, const String &hash); // with hash or signature check?
    // fine
    void copyFile(const Storage &remote_storage, const PackageId &pkg, const path &p, const StorageFileVerifier &v);

    // to remote

    // low level
    void copyFile(const path &p, const Storage &remote_storage);

    // low level
    void copyFile(const PackageId &pkg, const path &p, const Storage &remote_storage);

    // isn't it better?
    void getFile(); // alias for copy?
    void putFile(); // ?
};

// HttpsStorage; // read only
// RsyncStorage;?

}*/

static void checkPath(const path &p)
{
    const auto s = p.string();
    for (auto &c : s)
    {
        if (isspace(c))
            throw SW_RUNTIME_ERROR("You have spaces in the storage directory path. SW could not work in this directory: '" + s + "'");
    }
}

String toUserString(StorageFileType t)
{
    switch (t)
    {
    case StorageFileType::SourceArchive:
        return "Source Archive";
    default:
        SW_UNIMPLEMENTED;
    }
}

Directories::Directories(const path &p)
{
    auto make_canonical = [](const path &p)
    {
        auto a = fs::absolute(p);
        if (!fs::exists(a))
            fs::create_directories(a);
        return fs::canonical(a);
    };

    auto ap = make_canonical(p);
    checkPath(ap);

#ifdef _WIN32
    storage_dir = normalize_path_windows(ap);
#else
    storage_dir = ap.string();
#endif

#define DIR(x)                          \
    storage_dir_##x = storage_dir / #x; \
    fs::create_directories(storage_dir_##x);
#include "storage_directories.inl"
#undef SET
}

Storage::Storage(const String &name)
    : name(name)
{
}

StorageWithPackagesDatabase::StorageWithPackagesDatabase(const String &name, const path &db_dir)
    : Storage(name)
{
    pkgdb = std::make_unique<PackagesDatabase>(db_dir / name / "packages.db");
}

StorageWithPackagesDatabase::~StorageWithPackagesDatabase() = default;

PackageData StorageWithPackagesDatabase::loadData(const PackageId &id) const
{
    return pkgdb->getPackageData(id);
}

PackagesDatabase &StorageWithPackagesDatabase::getPackagesDatabase() const
{
    return *pkgdb;
}

void StorageWithPackagesDatabase::get(const IStorage &source, const PackageId &id, StorageFileType)
{
    SW_UNIMPLEMENTED;
}

std::unordered_map<UnresolvedPackage, Package>
StorageWithPackagesDatabase::resolve(const UnresolvedPackages &pkgs, UnresolvedPackages &unresolved_pkgs) const
{
    auto r = resolve_no_deps(pkgs, unresolved_pkgs);
    while (1)
    {
        auto r2 = r;
        auto sz = r.size();
        for (auto &[u, p] : r2)
            r.merge(resolve_no_deps(p.getData().dependencies, unresolved_pkgs));
        if (r.size() == sz)
            break;
    }
    return r;
}

std::unordered_map<UnresolvedPackage, Package>
StorageWithPackagesDatabase::resolve_no_deps(const UnresolvedPackages &pkgs, UnresolvedPackages &unresolved_pkgs) const
{
    std::unordered_map<UnresolvedPackage, Package> r;
    for (auto &[ud, pkg] : pkgdb->resolve(pkgs, unresolved_pkgs))
        r.emplace(ud, Package(*this, pkg));
    return r;
}

LocalStorage::LocalStorage(const path &local_storage_root_dir)
    : Directories(local_storage_root_dir), StorageWithPackagesDatabase("local", getDatabaseRootDir())
{
/*#define SW_CURRENT_LOCAL_STORAGE_VERSION 0
#define SW_CURRENT_LOCAL_STORAGE_VERSION_KEY "storage_version"
    auto version = sdb->getIntValue(SW_CURRENT_LOCAL_STORAGE_VERSION_KEY);
    if (version != SW_CURRENT_LOCAL_STORAGE_VERSION)
    {
        migrateStorage(version, SW_CURRENT_LOCAL_STORAGE_VERSION);
        sdb->setIntValue(SW_CURRENT_LOCAL_STORAGE_VERSION_KEY, version + 1);
    }*/

    pkgdb->open();
}

LocalStorage::~LocalStorage() = default;

path LocalStorage::getDatabaseRootDir() const
{
    return storage_dir_etc / "sw" / "database";
}

int LocalStorage::getHashSchemaVersion() const
{
    return 1;
}

int LocalStorage::getHashPathFromHashSchemaVersion() const
{
    return 2;
}

void LocalStorage::migrateStorage(int from, int to)
{
    if (to == from)
        return; // ok
    if (to < from)
        throw SW_RUNTIME_ERROR("Cannot migrate backwards");
    if (to - 1 > from)
        migrateStorage(from, to - 1);

    // close sdb first, reopen after

    switch (to)
    {
    case 1:
        throw SW_RUNTIME_ERROR("Not yet released");
        break;
    }
}

/*LocalPackage LocalStorage::download(const PackageId &id) const
{
    if (!getPackagesDatabase().isPackageInstalled(id))
        throw SW_RUNTIME_ERROR("package not installed: " + id.toString());
    return LocalPackage(*this, id);
}*/

LocalPackage LocalStorage::install(const Package &id) const
{
    if (!getPackagesDatabase().isPackageInstalled(id))
        throw SW_RUNTIME_ERROR("package not installed: " + id.toString());
    return LocalPackage(*this, id);
}

std::unique_ptr<vfs::File> LocalStorage::getFile(const PackageId &id, StorageFileType t) const
{
    SW_UNIMPLEMENTED;

    switch (t)
    {
    case StorageFileType::SourceArchive:
    {
        LocalPackage p(*this, id);
        auto d = p.getDirSrc() / make_archive_name();
        //return d.u8string();
    }
    default:
        SW_UNIMPLEMENTED;
    }
}

void LocalStorage::get(const IStorage &source, const PackageId &id, StorageFileType t)
{
    LocalPackage lp(*this, id);

    path dst;
    switch (t)
    {
    case StorageFileType::SourceArchive:
        dst = lp.getDirSrc() / make_archive_name();
        //dst += ".new"; // without this storage can be left in inconsistent state
        break;
    }

    LOG_INFO(logger, "Downloading: [" + id.toString() + "]/[" + toUserString(t) + "]");
    if (!source.getFile(id, t)->copy(dst))
        throw SW_RUNTIME_ERROR("Error downloading file for package: " + id.toString() + ", file: " + toUserString(t));

    LOG_INFO(logger, "Unpacking: [" + id.toString() + "]/[" + toUserString(t) + "]");
    unpack_file(dst, lp.getDirSrc());

    // now move .new to usual archive (or remove archive)
}

}
