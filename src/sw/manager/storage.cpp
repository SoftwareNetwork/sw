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

#include "storage.h"

#include "package_database.h"

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

String toUserString(StorageFileType t)
{
    switch (t)
    {
    case StorageFileType::SourceArchive:
        return "Source Archive";
    default:
        SW_UNREACHABLE;
    }
}

static path getDatabaseRootDir1(const path &root)
{
    return root / "sw" / "database";
}

path Directories::getDatabaseRootDir() const
{
    static const Strings upgrade_from
    {
        // push new values to front
        // "1"
    };

    auto p = getDatabaseRootDir1(storage_dir_etc) / "1";

    [[maybe_unused]]
    static bool once = [this, &new_root = p]()
    {
        for (auto &u : upgrade_from)
        {
            auto old = getDatabaseRootDir1(storage_dir_etc) / u;
            if (!fs::exists(old))
                continue;
            fs::copy(old, new_root, fs::copy_options::recursive);
            break;
        }
        return true;
    }();

    return p;
}

std::unordered_map<UnresolvedPackage, PackagePtr>
IStorage::resolveWithDependencies(const UnresolvedPackages &pkgs, UnresolvedPackages &unresolved_pkgs) const
{
    auto r = resolve(pkgs, unresolved_pkgs);
    while (1)
    {
        std::unordered_map<UnresolvedPackage, Package*> r2;
        for (auto &[u, p] : r)
            r2.emplace(u, p.get());
        auto sz = r.size();
        for (auto &[u, p] : r2)
            r.merge(resolve(p->getData().dependencies, unresolved_pkgs));
        if (r.size() == sz)
            break;
    }
    return r;
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

PackageDataPtr StorageWithPackagesDatabase::loadData(const PackageId &id) const
{
    std::lock_guard lk(m);
    auto i = data.find(id);
    if (i == data.end())
        return data.emplace(id, pkgdb->getPackageData(id)).first->second.clone();
    return i->second.clone();
}

PackagesDatabase &StorageWithPackagesDatabase::getPackagesDatabase() const
{
    return *pkgdb;
}

/*void StorageWithPackagesDatabase::get(const IStorage &source, const PackageId &id, StorageFileType)
{
    SW_UNIMPLEMENTED;
}*/

std::unordered_map<UnresolvedPackage, PackagePtr>
StorageWithPackagesDatabase::resolve(const UnresolvedPackages &pkgs, UnresolvedPackages &unresolved_pkgs) const
{
    std::unordered_map<UnresolvedPackage, PackagePtr> r;
    for (auto &[ud, pkg] : pkgdb->resolve(pkgs, unresolved_pkgs))
        r.emplace(ud, std::make_unique<Package>(*this, pkg));
    return r;
}

LocalStorageBase::LocalStorageBase(const String &name, const path &db_dir)
    : StorageWithPackagesDatabase(name, db_dir), schema(1, 2)
{
}

LocalStorageBase::~LocalStorageBase() = default;

std::unique_ptr<vfs::File> LocalStorageBase::getFile(const PackageId &id, StorageFileType t) const
{
    SW_UNIMPLEMENTED;

    switch (t)
    {
    case StorageFileType::SourceArchive:
    {
        //LocalPackage p(*this, id);
        //auto d = p.getDirSrc() / make_archive_name();
        //return d.u8string();
    }
    default:
        SW_UNREACHABLE;
    }
}

void LocalStorageBase::deletePackage(const PackageId &id) const
{
    getPackagesDatabase().deletePackage(id);
}

LocalStorage::LocalStorage(const path &local_storage_root_dir)
    : Directories(local_storage_root_dir)
    , LocalStorageBase("local", getDatabaseRootDir())
    , ovs(*this, getDatabaseRootDir())
{
/*#define SW_CURRENT_LOCAL_STORAGE_VERSION 0
#define SW_CURRENT_LOCAL_STORAGE_VERSION_KEY "storage_version"
    auto version = sdb->getIntValue(SW_CURRENT_LOCAL_STORAGE_VERSION_KEY);
    if (version != SW_CURRENT_LOCAL_STORAGE_VERSION)
    {
        migrateStorage(version, SW_CURRENT_LOCAL_STORAGE_VERSION);
        sdb->setIntValue(SW_CURRENT_LOCAL_STORAGE_VERSION_KEY, version + 1);
    }*/

    getPackagesDatabase().open();
}

LocalStorage::~LocalStorage() = default;

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

bool LocalStorage::isPackageInstalled(const Package &pkg) const
{
    LocalPackage p(*this, pkg);
    return getPackagesDatabase().isPackageInstalled(pkg) && fs::exists(p.getDirSrc2());
}

bool LocalStorage::isPackageLocal(const PackageId &id) const
{
    return id.getPath().isRelative();
}

bool LocalStorage::isPackageOverridden(const PackageId &pkg) const
{
    LocalPackage p(*this, pkg);
    return ovs.isPackageInstalled(p);
}

PackageDataPtr LocalStorage::loadData(const PackageId &id) const
{
    if (isPackageLocal(id))
    {
        auto i = local_packages.find(id);
        if (i == local_packages.end())
            throw SW_RUNTIME_ERROR("Missing local package data: " + id.toString());
        return std::make_unique<PackageData>(i->second);
    }
    if (isPackageOverridden(id))
        return ovs.loadData(id);
    return StorageWithPackagesDatabase::loadData(id);
}

LocalPackage LocalStorage::installLocalPackage(const PackageId &id, const PackageData &d)
{
    if (!isPackageLocal(id))
        throw SW_RUNTIME_ERROR("Not a local package: " + id.toString());
    local_packages[id] = d;
    LocalPackage p(*this, id);
    return p;
}

LocalPackage LocalStorage::install(const Package &id) const
{
    /*//if (&id.storage == this)
        //throw SW_RUNTIME_ERROR("Can't install from self to self");
    if (!isPackageInstalled(id))
        throw SW_RUNTIME_ERROR("package not installed: " + id.toString());
    return LocalPackage(*this, id);*/

    LocalPackage p(*this, id);
    if (isPackageInstalled(id) || isPackageOverridden(id))
        return p;

    // check if we already have this package and do not dl it again
    // and do not rewrite binaries etc.
    /*auto stamp = p.getDirSrc() / "aux" / "stamp.hash";
    if (fs::exists(stamp) && fs::exists(p.getDirSrc2()))
    {
        if (get_strong_file_hash(stamp) == read_file(stamp))
            ;
    }*/

    get(static_cast<const IStorage2 &>(id.getStorage()), id, StorageFileType::SourceArchive);

    // we mix gn with storage name to get unique gn
    /*auto h = std::hash<String>()(static_cast<const IStorage2 &>(id.getStorage()).getName());
    auto d = id.getData();
    d.group_number = hash_combine(h, d.group_number);*/

    getPackagesDatabase().installPackage(id, id.getData());
    return p;
}

void LocalStorage::get(const IStorage2 &source, const PackageId &id, StorageFileType t) const
{
    LocalPackage lp(*this, id);

    path dst;
    switch (t)
    {
    case StorageFileType::SourceArchive:
        dst = lp.getDir() / make_archive_name();
        //dst += ".new"; // without this storage can be left in inconsistent state
        break;
    }

    LOG_INFO(logger, "Downloading: [" + id.toString() + "]/[" + toUserString(t) + "]");
    auto f = source.getFile(id, t);
    if (!f->copy(dst))
        throw SW_RUNTIME_ERROR("Error downloading file for package: " + id.toString() + ", file: " + toUserString(t));

    SCOPE_EXIT
    {
        // now move .new to usual archive (or remove archive)
        // we're removing for now
        fs::remove(dst);
    };

    auto unpack = [&id, &dst, &lp, &t]()
    {
        for (auto &d : fs::directory_iterator(lp.getDir()))
        {
            if (d.path() != dst)
                fs::remove_all(d);
        }

        LOG_INFO(logger, "Unpacking  : [" + id.toString() + "]/[" + toUserString(t) + "]");
        unpack_file(dst, lp.getDirSrc());
    };

    // at the moment we perform check after download
    // but maybe we can move it before real download?
    if (auto fh = dynamic_cast<const vfs::FileWithHashVerification *>(f.get()))
    {
        if (fh->getHash() == lp.getStampHash())
        {
            // skip unpack
            return;
        }

        unpack();
        write_file(lp.getStampFilename(), fh->getHash());
        return;
    }

    unpack();
}

OverriddenPackagesStorage &LocalStorage::getOverriddenPackagesStorage()
{
    return ovs;
}

const OverriddenPackagesStorage &LocalStorage::getOverriddenPackagesStorage() const
{
    return ovs;
}

std::unordered_map<UnresolvedPackage, PackagePtr>
LocalStorage::resolve(const UnresolvedPackages &pkgs, UnresolvedPackages &unresolved_pkgs) const
{
    return ovs.resolve(pkgs, unresolved_pkgs);
}

void LocalStorage::remove(const LocalPackage &p) const
{
    getPackagesDatabase().deletePackage(p);
    error_code ec;
    fs::remove_all(p.getDir(), ec);
}

OverriddenPackagesStorage::OverriddenPackagesStorage(const LocalStorage &ls, const path &db_dir)
    : LocalStorageBase("overridden", db_dir), ls(ls)
{
    getPackagesDatabase().open();
}

OverriddenPackagesStorage::~OverriddenPackagesStorage() = default;

std::unordered_set<LocalPackage> OverriddenPackagesStorage::getPackages() const
{
    std::unordered_set<LocalPackage> pkgs;
    for (auto &id : getPackagesDatabase().getOverriddenPackages())
        pkgs.emplace(ls, id);
    return pkgs;
}

void OverriddenPackagesStorage::deletePackageDir(const path &sdir) const
{
    getPackagesDatabase().deleteOverriddenPackageDir(sdir);
}

LocalPackage OverriddenPackagesStorage::install(const Package &p) const
{
    // we can't install from ourselves
    if (&p.getStorage() == this)
        return LocalPackage(ls, p);

    // we mix gn with storage name to get unique gn
    /*auto h = std::hash<String>()(static_cast<const IStorage2 &>(p.getStorage()).getName());
    auto d = p.getData();
    d.group_number = hash_combine(h, d.group_number);*/

    return install(p, p.getData());
}

LocalPackage OverriddenPackagesStorage::install(const PackageId &id, const PackageData &d) const
{
    getPackagesDatabase().installPackage(id, d);
    return LocalPackage(ls, id);
}

bool OverriddenPackagesStorage::isPackageInstalled(const Package &p) const
{
    return getPackagesDatabase().getInstalledPackageId(p) != 0;
}

std::unordered_map<UnresolvedPackage, PackagePtr>
CachedStorage::resolve(const UnresolvedPackages &pkgs, UnresolvedPackages &unresolved_pkgs) const
{
    std::unordered_map<UnresolvedPackage, PackagePtr> r;
    for (auto &u : pkgs)
    {
        auto i = resolved_packages.find(u);
        if (i == resolved_packages.end())
        {
            unresolved_pkgs.insert(u);
            continue;
        }
        r.emplace(u, i->second->clone());
    }
    return r;
}

void CachedStorage::storePackages(const StoredPackages &pkgs)
{
    for (auto &[u,p] : pkgs)
        resolved_packages.emplace(u, p->clone());
}

}
