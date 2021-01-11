// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>

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

/*String toUserString(StorageFileType t)
{
    switch (t)
    {
    case StorageFileType::SourceArchive:
        return "Source Archive";
    default:
        SW_UNREACHABLE;
    }
}*/

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

/*PackageDataPtr StorageWithPackagesDatabase::loadData(const PackageId &id) const
{
    std::lock_guard lk(m);
    auto i = data.find(id);
    if (i == data.end())
        return data.emplace(id, pkgdb->getPackageData(id)).first->second.clone();
    return i->second.clone();
}*/

PackagesDatabase &StorageWithPackagesDatabase::getPackagesDatabase() const
{
    return *pkgdb;
}

bool StorageWithPackagesDatabase::resolve(ResolveRequest &rr) const
{
    return pkgdb->resolve(rr, *this, false);
}

LocalStorageBase::LocalStorageBase(const String &name, const path &db_dir)
    : StorageWithPackagesDatabase(name, db_dir), schema(1, 2)
{
}

LocalStorageBase::~LocalStorageBase() = default;

void LocalStorageBase::deletePackage(const PackageId &id) const
{
    getPackagesDatabase().deletePackage(id);
}

LocalStorage::LocalStorage(const path &local_storage_root_dir)
    : Directories(local_storage_root_dir)
    , LocalStorageBase("local", getDatabaseRootDir())
    //, ovs(*this, getDatabaseRootDir())
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

bool LocalStorage::isPackageInstalled(const Package &pkg) const
{
    return getPackagesDatabase().isPackageInstalled(pkg);
}

bool LocalStorage::isPackageLocal(const PackageId &id) const
{
    SW_UNIMPLEMENTED;
    //return id.getPath().isRelative();
}

LocalPackage LocalStorage::installLocalPackage(const PackageId &id, const PackageData &d)
{
    SW_UNIMPLEMENTED;
    /*if (!isPackageLocal(id))
        throw SW_RUNTIME_ERROR("Not a local package: " + id.toString());
    local_packages.emplace(id, d);
    LocalPackage p(*this, id);
    return p;*/
}

path getHashPathFromHash(const String &h, int nsubdirs, int chars_per_subdir)
{
    path p;
    int i = 0;
    for (; i < nsubdirs; i++)
        p /= h.substr(i * chars_per_subdir, chars_per_subdir);
    p /= h.substr(i * chars_per_subdir);
    return p;
}

String getHash(const PackageName &n)
{
    // move these calculations to storage?
    //switch (1)
    {
    //case 1:
        return blake2b_512(n.getPath().toStringLower() + "-" + n.getVersion().toString());
    }
}

static path get_lp_root_dir(const path &root, const PackageId &id)
{
    auto fn = root;
    fn /= getHashPathFromHash(shorten_hash(getHash(id.getName()), 8), 2, 2);
    return fn;
}

static path get_lp_pkg_dir(const path &root, const Package &p)
{
    auto fn = get_lp_root_dir(root, p.getId());
    fn /= "p";
    fn /= p.getId().getSettings().getHashString();
    return fn;
}

static path get_lp_dir2_dir(const path &root, const Package &p)
{
    auto fn = get_lp_pkg_dir(root, p.getId());
    return fn / getSourceDirectoryName();
}

bool LocalStorage::resolve(ResolveRequest &rr) const
{
    auto r = StorageWithPackagesDatabase::resolve(rr);
    if (r)
    {
        auto d = get_lp_pkg_dir(storage_dir_pkg, rr.getPackage().getId());
        if (!fs::exists(d))
        {
            rr.r.reset();
            return false;
        }
    }
    return r;
}

std::unique_ptr<Package> LocalStorage::install(const Package &p) const
{
    if (!p.isInstallable())
        return {};

    /*//if (&id.storage == this)
        //throw SW_RUNTIME_ERROR("Can't install from self to self");
    if (!isPackageInstalled(id))
        throw SW_RUNTIME_ERROR("package not installed: " + id.toString());
    return LocalPackage(*this, id);*/

    auto fn = get_lp_root_dir(storage_dir_pkg, p.getId());
    fn /= p.getId().getSettings().getHashString() + ".tar.gz";

    auto dst = get_lp_pkg_dir(storage_dir_pkg, p.getId());

    if (isPackageInstalled(p)
        && fs::exists(dst) // unnecessary check?
        )
    {
        auto pkg = makePackage(p.getId());
        auto d = std::make_unique<PackageData>(p.getData());
        d->sdir = get_lp_pkg_dir(storage_dir_pkg, p.getId());
        pkg->setData(std::move(d));
        return pkg;
    }
    fs::create_directories(dst);

    auto h = p.getId().getSettings().getHash();
    auto settings_name = std::to_string(h);
    if (h == 0)
        settings_name = "Source Archive";

    LOG_INFO(logger, "Downloading: [" + p.getId().toString() + "]/["
        //+ toUserString(t)
        + settings_name
        + "]");
    p.copyArchive(fn);
    //get(static_cast<const IStorage2 &>(p.getStorage()), p);

    SCOPE_EXIT
    {
        // now move .new to usual archive (or remove archive)
        // we're removing for now
        fs::remove(fn);
    };

    // unpack
    for (auto &d : fs::directory_iterator(dst))
    {
        if (d.path() != fn)
            fs::remove_all(d);
    }

    LOG_INFO(logger, "Unpacking  : [" + p.getId().toString() + "]/["
        //+ toUserString(t)
        + settings_name
        + "]");
    unpack_file(fn, dst);

    //
    getPackagesDatabase().installPackage(p);

    auto pkg = makePackage(p.getId());
    auto d = std::make_unique<PackageData>(p.getData());
    d->sdir = get_lp_pkg_dir(storage_dir_pkg, p.getId());
    pkg->setData(std::move(d));
    return pkg;
}

std::unique_ptr<Package> LocalStorage::makePackage(const PackageId &id) const
{
    struct LocalPackage2 : Package
    {
        path sdir;

        LocalPackage2(const Package &id, const LocalStorage &s)
            : Package(id)
        {
            sdir = get_lp_pkg_dir(s.storage_dir_pkg, getId());
        }

        bool isInstallable() const override { return false; }
        std::unique_ptr<Package> clone() const override { return std::make_unique<LocalPackage2>(*this); }
        path getDirSrc2() const override { return sdir; }
    };

    return std::make_unique<LocalPackage2>(id, *this);
}

void LocalStorage::remove(const LocalPackage &p) const
{
    SW_UNIMPLEMENTED;
    /*getPackagesDatabase().deletePackage(p);
    error_code ec;
    fs::remove_all(p.getDir(), ec);*/
}

OverriddenPackagesStorage::OverriddenPackagesStorage(/*const LocalStorage &ls, */const path &db_dir)
    : LocalStorageBase("overridden", db_dir)
    //, ls(ls)
{
    getPackagesDatabase().open();
}

OverriddenPackagesStorage::~OverriddenPackagesStorage() = default;

std::unique_ptr<Package> OverriddenPackagesStorage::makePackage(const PackageId &id) const
{
    struct OverriddenPackage2 : Package
    {
        using Package::Package;

        bool isInstallable() const override { return false; }
        std::unique_ptr<Package> clone() const override { return std::make_unique<OverriddenPackage2>(*this); }
        path getDirSrc2() const override { return getData().sdir; }
    };

    auto p = std::make_unique<OverriddenPackage2>(id);
    return p;
}

std::unordered_set<LocalPackage> OverriddenPackagesStorage::getPackages() const
{
    SW_UNIMPLEMENTED;
    /*std::unordered_set<LocalPackage> pkgs;
    for (auto &id : getPackagesDatabase().getOverriddenPackages())
        pkgs.emplace(ls, id);
    return pkgs;*/
}

void OverriddenPackagesStorage::deletePackageDir(const path &sdir) const
{
    getPackagesDatabase().deleteOverriddenPackageDir(sdir);
}

LocalPackage OverriddenPackagesStorage::install(const PackageId &id, const PackageData &d) const
{
    SW_UNIMPLEMENTED;
    //getPackagesDatabase().installPackage(id, d);
    //return LocalPackage(ls, id);
}

bool OverriddenPackagesStorage::isPackageInstalled(const Package &p) const
{
    SW_UNIMPLEMENTED;
    //return getPackagesDatabase().getInstalledPackageId(p) != 0;
}

void CachedStorage::storePackages(const ResolveRequest &rr)
{
    std::unique_lock lk(m);
    SW_CHECK(rr.isResolved());
    resolved_packages[rr.u][rr.settings.getHash()] = Value{ rr.getPackage().clone() };
}

bool CachedStorage::resolve(ResolveRequest &rr) const
{
    std::shared_lock lk(m);
    auto i = resolved_packages.find(rr.u);
    if (i == resolved_packages.end())
        return false;
    auto j = i->second.find(rr.settings.getHash());
    if (j == i->second.end())
        return false;
    rr.setPackage(j->second.r->clone());
    return true;
}

void CachedStorage::clear()
{
    resolved_packages.clear();
}

CachingResolver::CachingResolver(CachedStorage &cache)
    : cache(cache)
{
}

bool CachingResolver::resolve(ResolveRequest &rr) const
{
    if (cache.resolve(rr))
        return true;
    if (Resolver::resolve(rr))
    {
        cache.storePackages(rr);
        return true;
    }
    return false;
}

}
