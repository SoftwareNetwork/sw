// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "resolver.h"

#include "api.h"
#include "database.h"
#include "directories.h"
#include "exceptions.h"
#include "lock.h"
#include "property_tree.h"
#include "settings.h"
#include "sqlite_database.h"
#include "yaml.h"

#include <boost/algorithm/string.hpp>

#include <primitives/executor.h>
#include <primitives/hash.h>
#include <primitives/hasher.h>
#include <primitives/pack.h>
#include <primitives/templates.h>
#include <primitives/win32helpers.h>
#include <primitives/sw/settings.h>
#include <nlohmann/json.hpp>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "resolver");

TYPED_EXCEPTION(LocalDbHashException);
TYPED_EXCEPTION(DependencyNotResolved);

bool gForceServerQuery;
static cl::opt<bool, true> force_server_query1("s", cl::desc("Force server check"), cl::location(gForceServerQuery));
static cl::alias force_server_query2("server", cl::desc("Alias for -s"), cl::aliasopt(force_server_query1));

bool gVerbose;
static cl::opt<bool, true> verbose_opt("verbose", cl::desc("Verbose output"), cl::location(gVerbose));
static cl::alias verbose_opt2("v", cl::desc("Alias for -verbose"), cl::aliasopt(verbose_opt));

bool gUseLockFile;
static cl::opt<bool, true> use_lock_file("l", cl::desc("Use lock file"), cl::location(gUseLockFile));// , cl::init(true));

#define SW_CURRENT_LOCK_FILE_VERSION 1

namespace sw
{

Resolver::Dependencies getDependenciesFromRemote(const UnresolvedPackages &deps, const Remote *current_remote);
Resolver::Dependencies getDependenciesFromDb(const UnresolvedPackages &deps, const Remote *current_remote);
Resolver::Dependencies prepareIdDependencies(const IdDependencies &id_deps, const Remote *current_remote);

PackageStore &getPackageStore()
{
    static PackageStore rd;
    return rd;
}

void PackageStore::clear()
{
    *this = PackageStore();
}

std::optional<ExtendedPackageData> PackageStore::isPackageResolved(const UnresolvedPackage &p)
{
    auto i = resolved_packages.find(p);
    if (i == resolved_packages.end())
        return {};
    return i->second;
}

void PackageStore::loadLockFile(const path &fn)
{
    auto j = nlohmann::json::parse(read_file(fn));
    if (j["version"] != SW_CURRENT_LOCK_FILE_VERSION)
    {
        throw SW_RUNTIME_ERROR("Cannot use this lock file: bad version " + std::to_string((int)j["version"]) +
            ", expected " + std::to_string(SW_CURRENT_LOCK_FILE_VERSION));
    }

    const auto &overridden = getServiceDatabase().getOverriddenPackages();

    for (auto &v : j["packages"])
    {
        DownloadDependency d;
        (PackageId&)d = extractFromStringPackageId(v["package"].get<std::string>());
        //d.createNames();
        d.prefix = v["prefix"];
        d.hash = v["hash"];
        d.group_number_from_lock_file = d.group_number = v["group_number"];
        auto i = overridden.find(d);
        d.local_override = i != overridden.end(d);
        if (d.local_override)
            d.group_number = i->second.getGroupNumber();
        d.from_lock_file = true;
        for (auto &v2 : v["dependencies"])
        {
            auto p = extractFromStringPackageId(v2.get<std::string>());
            DownloadDependency1 d2{ p };
            d.db_dependencies[p.ppath.toString()] = d2;
        }
        download_dependencies_.insert(d);
    }

    for (auto &v : j["resolved_packages"].items())
    {
        auto p = extractFromString(v.key());
        DownloadDependency d;
        (PackageId&)d = extractFromStringPackageId(v.value()["package"].get<std::string>());
        auto i = download_dependencies_.find(d);
        if (i == download_dependencies_.end())
            throw SW_RUNTIME_ERROR("bad lock file");
        d = *i;
        if (v.value().find("installed") != v.value().end())
            d.installed = v.value()["installed"];
        resolved_packages[p] = d;
    }

    use_lock_file = true;
}

void PackageStore::saveLockFile(const path &fn) const
{
    if (download_dependencies_.empty() && resolved_packages.empty())
        return;

    nlohmann::json j;
    j["version"] = SW_CURRENT_LOCK_FILE_VERSION;

    auto &jpkgs = j["packages"];
    for (auto &r : std::set<DownloadDependency>(download_dependencies_.begin(), download_dependencies_.end()))
    {
        nlohmann::json jp;
        jp["package"] = r.toString();
        jp["prefix"] = r.prefix;
        jp["hash"] = r.hash;
        if (r.group_number > 0)
            jp["group_number"] = r.group_number;
        else
            jp["group_number"] = r.group_number_from_lock_file;
        for (auto &[_, d] : std::map<String, DownloadDependency1>(r.db_dependencies.begin(), r.db_dependencies.end()))
            jp["dependencies"].push_back(d.toString());
        jpkgs.push_back(jp);
    }

    auto &jp = j["resolved_packages"];
    for (auto &[u, r] : std::map<UnresolvedPackage, DownloadDependency>(resolved_packages.begin(), resolved_packages.end()))
    {
        jp[u.toString()]["package"] = r.toString();
        if (r.installed)
            jp[u.toString()]["installed"] = true;
    }

    write_file_if_different(fn, j.dump(2));
}

bool PackageStore::canUseLockFile() const
{
    return use_lock_file && !gForceServerQuery && gUseLockFile;
}

ResolvedPackagesMap resolve_dependencies(const UnresolvedPackages &deps)
{
    Resolver r;
    r.resolve_dependencies(deps);
    return r.resolved_packages;
}

std::unordered_set<ExtendedPackageData> resolveAllDependencies(const UnresolvedPackages &deps)
{
    Resolver r;
    r.resolve_dependencies(deps);
    return r.getDownloadDependencies();
}

void resolve_and_download(const UnresolvedPackage &p, const path &fn)
{
    Resolver r;
    r.resolve_and_download(p, fn);
}

std::unordered_set<ExtendedPackageData> Resolver::getDownloadDependencies() const
{
    std::unordered_set<ExtendedPackageData> s;
    for (auto &dl : download_dependencies_)
        s.insert(dl);
    return s;
}

std::unordered_map<ExtendedPackageData, PackageVersionGroupNumber> Resolver::getDownloadDependenciesWithGroupNumbers() const
{
    std::unordered_map<ExtendedPackageData, PackageVersionGroupNumber> s;
    for (auto &dl : download_dependencies_)
        s[dl] = dl.group_number;
    return s;
}

void Resolver::resolve_dependencies(const UnresolvedPackages &dependencies, bool clean_resolve)
{
    UnresolvedPackages deps;
    UnresolvedPackages known_deps;

    // remove some packages
    for (auto &d : dependencies)
    {
        // remove local packages
        if (d.ppath.is_loc())
            continue;

        if (!clean_resolve)
        {
            // remove already downloaded packages
            auto i = getPackageStore().resolved_packages.find(d);
            if (i != getPackageStore().resolved_packages.end())
            {
                resolved_packages[d] = i->second;
                known_deps.insert(d);
                continue;
            }
        }

        deps.insert(d);
    }

    if (deps.empty())
        return;

    resolve(deps, [this] { download_and_unpack(); });

    // add back known_deps
    for (auto &d : known_deps)
        download_dependencies_.insert(resolved_packages[d]);

    // mark packages as resolved
    for (auto &d : deps)
    {
        for (auto &dl : download_dependencies_)
        {
            /*if (!dl.flags[pfDirectDependency])
                continue;*/
            if (d.ppath == dl.ppath)
            {
                resolved_packages[d] = dl;
                continue;
            }
            // if this is not exact match, assign to self
            // TODO: or make resolved_packages multimap

            // we do not allow d.ppath.isRootOf() here as it was in cppan
            //if (d.ppath.isRootOf(dl.second.ppath))
                //resolved_packages[d] = dl.second;
        }
    }

    getPackageStore().resolved_packages.insert(resolved_packages.begin(), resolved_packages.end());
    getPackageStore().download_dependencies_.insert(download_dependencies_.begin(), download_dependencies_.end());
}

void Resolver::resolve_and_download(const UnresolvedPackage &p, const path &fn)
{
    resolve({ p }, [&]
    {
        for (auto &dd : download_dependencies_)
        {
            //if (dd.second == p)
            {
                download(dd, fn);
                break;
            }
        }
    });
}

void Resolver::add_dep(Dependencies &dd, const PackageId &d)
{
    DownloadDependency d2;
    (PackageId&)d2 = d;
    auto i = getPackageStore().download_dependencies_.find(d2);
    if (i == getPackageStore().download_dependencies_.end())
        throw SW_RUNTIME_ERROR("unresolved package from lock file: " + d.toString());
    if (!dd.insert(*i).second)
        return;
    for (auto &d : i->db_dependencies)
        add_dep(dd, d.second);
}

void Resolver::resolve(const UnresolvedPackages &deps, std::function<void()> resolve_action)
{
    if (getPackageStore().canUseLockFile())
    {
        UnresolvedPackages deps2;
        for (auto &d : deps)
        {
            auto &up = getPackageStore().resolved_packages;
            auto i = up.find(d);
            if (i == up.end())
            {
                deps2.insert(d);
                LOG_INFO(logger, "new dependency detected: " + d.toString());
                //throw SW_RUNTIME_ERROR("unresolved package from lock file: " + d.toString());
                continue;
            }
            add_dep(download_dependencies_, i->second);
        }
        if (!deps2.empty())
            resolve1(deps2, resolve_action);
        resolve_action();
        return;
    }

    resolve1(deps, resolve_action);
}

void Resolver::resolve1(const UnresolvedPackages &deps, std::function<void()> resolve_action)
{
    if (!resolve_action)
        throw std::logic_error("Empty resolve action!");

    // ref to not invalidate all ptrs
    auto &us = Settings::get_user_settings();
    auto cr = us.remotes.begin();
    current_remote = &*cr++;

    auto merge_dd = [this](auto &dd)
    {
        for (auto &d : dd)
        {
            download_dependencies_.erase(d);
            download_dependencies_.insert(d);
        }
        //download_dependencies_.insert(dd.begin(), dd.end());
    };

    auto resolve_remote_deps = [this, &deps, &cr, &us, &merge_dd]()
    {
        bool again = true;
        while (again)
        {
            again = false;
            try
            {
                if (us.remotes.size() > 1)
                    LOG_INFO(logger, "Trying " + current_remote->name + " remote");
                auto dd = getDependenciesFromRemote(deps, current_remote);
                merge_dd(dd);
            }
            catch (const std::exception &e)
            {
                LOG_WARN(logger, e.what());
                if (cr != us.remotes.end())
                {
                    current_remote = &*cr++;
                    again = true;
                }
                else
                    throw DependencyNotResolved();
            }
        }
    };

    //query_local_db = !gForceServerQuery;
    // do 2 attempts: 1) local db, 2) remote db
    //int n_attempts = query_local_db ? 2 : 1;
    int n_attempts = gForceServerQuery ? 1 : 2;
    while (n_attempts--)
    {
        try
        {
            //if (query_local_db)
            if (!gForceServerQuery)
            {
                try
                {
                    auto dd = getDependenciesFromDb(deps, current_remote);
                    merge_dd(dd);
                }
                catch (std::exception &e)
                {
                    LOG_ERROR(logger, "Cannot get dependencies from local database: " << e.what());

                    //query_local_db = false;
                    gForceServerQuery = true;
                    resolve_remote_deps();
                }
            }
            else
            {
                resolve_remote_deps();
            }

            resolve_action();
        }
        catch (LocalDbHashException &)
        {
            LOG_WARN(logger, "Local db data caused issues, trying remote one");

            //query_local_db = false;
            gForceServerQuery = true;
            continue;
        }
        break;
    }
}

void Resolver::download(const ExtendedPackageData &d, const path &fn)
{
    auto provs = getPackagesDatabase().getDataSources();
    if (provs.empty())
        throw SW_RUNTIME_ERROR("No data sources available");

    if (std::none_of(provs.begin(), provs.end(),
        [&](auto &prov) {return prov.downloadPackage(d, d.hash, fn, /*query_local_db*/ !gForceServerQuery);}))
    {
        // if we get hashes from local db
        // they can be stalled within server refresh time (15 mins)
        // in this case we should do request to server
        auto err = "Hashes do not match for package: " + d.toString();
        //if (query_local_db)
        if (!gForceServerQuery)
            throw LocalDbHashException(err);
        throw SW_RUNTIME_ERROR(err);
    }
}

void Resolver::download_and_unpack()
{
    if (download_dependencies_.empty())
        return;

    auto download_dependency = [this](auto &dd) mutable
    {
        auto &d = dd;
        auto version_dir = d.getDirSrc();
        auto hash_file = d.getStampFilename();
        auto stampfile_hash = d.getStampHash();
        bool must_download = stampfile_hash != d.hash || d.hash.empty();

        auto &sdb = getServiceDatabase();

        if (d.local_override)
            return;
        if (fs::exists(version_dir) && sdb.isPackageInstalled(d))
        {
            if (!must_download)
                return;
            if (d.from_lock_file)
            {
                ((DownloadDependency&)d).hash = stampfile_hash;
                return;
            }
        }

        // lock, so only one cppan process at the time could download the project
        ScopedFileLock lck(hash_file, std::defer_lock);
        if (!lck.try_lock())
        {
            // download is in progress, wait and register config
            ScopedFileLock lck2(hash_file);
            return;
        }

        // Do this before we clean previous package version!
        // This is useful when we have network issues during download,
        // so we won't lost existing package.
        LOG_INFO(logger, "Downloading: " << d.toString() << "...");

        // maybe d.target_name instead of version_dir.string()?
        path fn = make_archive_name((temp_directory_path("dl") / d.toString()).string());
        download(d, fn);

        // verify before cleaning old pkg
        //if (Settings::get_local_settings().verify_all)
        //    verify(d, fn);

        // remove existing version dir
        // cleanPackages(d.target_name);

        // remove all old possible created files, not more needed files etc.
        fs::remove_all(d.getDir());
        fs::remove_all(d.getDirObj()); // manually delete obj dir

        LOG_INFO(logger, "Unpacking  : " << d.toString() << "...");
        Files files;
        try
        {
            files = unpack_file(fn, version_dir);

            // write hash file after unpack
            write_file(hash_file, d.hash);
        }
        catch (...)
        {
            fs::remove(fn);
            fs::remove_all(d.getDir());
            throw;
        }
        fs::remove(fn);

        // install package
        sdb.addInstalledPackage(d, d.group_number);

        auto create_link = [](const auto &p, const auto &ln)
        {
            if (!fs::exists(ln))
                ::create_link(p, ln, "CPPAN link");
        };

#ifdef _WIN32
        create_link(d.getDirSrc().parent_path(), getUserDirectories().storage_dir_lnk / "src" / (d.toString() + ".lnk"));
        //create_link(d.getDirObj(), directories.storage_dir_lnk / "obj" / (cc.first.target_name + ".lnk"));
#endif
    };

    //Executor e(1);
    auto &e = getExecutor();
    std::vector<Future<void>> fs;

    // threaded execution does not preserve object creation/destruction order,
    // so current path is not correctly restored
    ScopedCurrentPath cp;

    for (auto &dd : download_dependencies_)
        fs.push_back(e.push([&download_dependency, &dd] { download_dependency(dd); }));
    waitAndGet(fs);

    // two following blocks use executor to do parallel queries
    //if (query_local_db)
    if (!gForceServerQuery && add_downloads)
    {
        // send download list
        // remove this when cppan will be widely used
        // also because this download count can be easily abused
        e.push([this]()
        {
            if (!current_remote)
                return;

            std::set<int64_t> ids;
            for (auto &d : download_dependencies_)
            {
                if (d.local_override)
                    continue;
                ids.insert(d.id);
            }

            try
            {
                Api api(*current_remote);
                api.addDownloads(ids);
            }
            catch (...)
            {
            }
        });
    }

    // send download action once
    RUN_ONCE
    {
        if (add_downloads)
        e.push([this]
        {
            if (!current_remote)
                return;
            try
            {
                Api api(*current_remote);
                api.addClientCall();
            }
            catch (...)
            {
            }
        });
    };

    e.wait();
}

Resolver::Dependencies getDependenciesFromRemote(const UnresolvedPackages &deps, const Remote *current_remote)
{
    Api api(*current_remote);

    IdDependencies id_deps;

    LOG_INFO(logger, "Requesting dependency list... ");
    {
        int ct = 5;
        int t = 10;
        int n_tries = 3;
        while (1)
        {
            try
            {
                id_deps = api.resolvePackages(deps);
                break;
            }
            catch (...)
            {
                throw;
                LOG_INFO(logger, "Retrying... ");
            }
        }
    }

    // dependencies were received without error

    // we might get less packages than requested if we request same packages, so detect duplicates
    std::unordered_map<PackagePath, int> dups;
    for (auto &d : deps)
        dups[d.ppath]++;

    // set id dependencies
    int unresolved = (int)deps.size();
    for (auto &v : id_deps)
    {
        unresolved -= dups[v.second.ppath];
        dups[v.second.ppath] = 0;
    }

    if (unresolved > 0)
    {
        UnresolvedPackages pkgs;
        for (auto &[d, n] : dups)
        {
            if (n == 0)
                continue;
            for (auto &dep : deps)
            {
                if (dep.ppath == d)
                    pkgs.insert(dep);
            }
        }

        /*try
        {
            //getPackagesDatabase().findLocalDependencies(id_deps, pkgs);
            throw;
        }
        catch (...)
        {*/
            String s;
            for (auto &d : pkgs)
                s += d.toString() + ", ";
            if (!s.empty())
                s.resize(s.size() - 2);
            throw SW_RUNTIME_ERROR("Some packages (" + std::to_string(unresolved) + ") are unresolved: " + s);
        //}
    }

    return prepareIdDependencies(id_deps, current_remote);
}

Resolver::Dependencies getDependenciesFromDb(const UnresolvedPackages &deps, const Remote *current_remote)
{
    return prepareIdDependencies(getPackagesDatabase().findDependencies(deps), current_remote);
}

Resolver::Dependencies prepareIdDependencies(const IdDependencies &id_deps, const Remote *current_remote)
{
    Resolver::Dependencies dependencies;
    for (auto &v : id_deps)
    {
        auto d = v.second;
        //d.createNames();
        d.remote = current_remote;
        d.prepareDependencies(id_deps);
        d.db_dependencies = d.db_dependencies;
        dependencies.insert(d);
    }
    return dependencies;
}

Packages resolve_dependency(const String &target_name)
{
    String target = target_name;
    bool added_suffix = false;
    if (target.find('-') == target.npos)
    {
        target += "-*"; // add the latest version
        added_suffix = true;
    }

    auto p = extractFromString(target);
    Packages pkgs;
    ResolvedPackagesMap pkgs2;
    try
    {
        pkgs2 = resolve_dependencies({ p });
    }
    catch (const sw::RuntimeError &)
    {
        if (!added_suffix)
            throw;

        target = target_name + "-master"; // add the master version
        p = extractFromString(target);
        pkgs2 = resolve_dependencies({ p });

        // TODO: if no master version, try to get first branch from local db
        // (another try ... catch)
        //target = target_name + "-master"; // add the master version
        //p = extractFromString(target);
        //resolved_deps = resolve_dependencies({ { p.ppath.toString(), p } });
    }
    for (auto &pkg : pkgs2)
        pkgs.insert(pkg.second);
    return pkgs;
}

}
