// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "resolver.h"

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

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "resolver");

#define CURRENT_API_LEVEL 1
#define DEPENDENCIES_NODE "dependencies"

TYPED_EXCEPTION(LocalDbHashException);
TYPED_EXCEPTION(DependencyNotResolved);

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

// legacy varname rd - was: response data
PackageStore rd;

PackageStore::PackageConfig &PackageStore::operator[](const Package &p)
{
    return packages[p];
}

const PackageStore::PackageConfig &PackageStore::operator[](const Package &p) const
{
    auto i = packages.find(p);
    if (i == packages.end())
        throw std::runtime_error("Package not found: " + p.getTargetName());
    return i->second;
}

bool PackageStore::has_local_package(const PackagePath &ppath) const
{
    return local_packages.find(ppath) != local_packages.end();
}

path PackageStore::get_local_package_dir(const PackagePath &ppath) const
{
    auto i = local_packages.find(ppath);
    if (i != local_packages.end())
        return i->second;
    return path();
}

ResolverPackagesMap resolve_dependencies(const UnresolvedPackages &deps)
{
    Resolver r;
    r.resolve_dependencies(deps);
    return r.resolved_packages;
}

Packages resolveAllDependencies(const UnresolvedPackages &deps)
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

Packages Resolver::getDownloadDependencies() const
{
    Packages s;
    for (auto &dl : download_dependencies_)
        s.insert(dl.first);
    return s;
}

std::unordered_map<Package, PackageVersionGroupNumber> Resolver::getDownloadDependenciesWithGroupNumbers() const
{
    std::unordered_map<Package, PackageVersionGroupNumber> s;
    for (auto &dl : download_dependencies_)
        s[dl.first] = dl.second.group_number;
    return s;
}

void Resolver::resolve_dependencies(const UnresolvedPackages &dependencies)
{
    UnresolvedPackages deps;

    // remove some packages
    for (auto &d : dependencies)
    {
        // remove local packages
        if (d.ppath.is_loc())
            continue;

        // remove already downloaded packages
        auto i = getPackageStore().resolved_packages.find(d);
        if (i != getPackageStore().resolved_packages.end())
        {
            resolved_packages[d] = i->second;
            continue;
        }

        deps.insert(d);
    }

    if (deps.empty())
        return;

    resolve(deps, [this] { download_and_unpack(); });

    // mark packages as resolved
    for (auto &d : deps)
    {
        for (auto &dl : download_dependencies_)
        {
            if (!dl.second.flags[pfDirectDependency])
                continue;
            if (d.ppath == dl.second.ppath)
            {
                resolved_packages[d] = dl.second;
                continue;
            }
            // if this is not exact match, assign to self
            // TODO: or make resolved_packages multimap
            if (d.ppath.isRootOf(dl.second.ppath))
                resolved_packages[d] = dl.second;
        }
    }

    getPackageStore().resolved_packages.insert(resolved_packages.begin(), resolved_packages.end());
}

void Resolver::resolve_and_download(const UnresolvedPackage &p, const path &fn)
{
    resolve({ p }, [&]
    {
        for (auto &dd : download_dependencies_)
        {
            //if (dd.second == p)
            {
                download(dd.second, fn);
                break;
            }
        }
    });
}

void Resolver::resolve(const UnresolvedPackages &deps, std::function<void()> resolve_action)
{
    if (!resolve_action)
        throw std::logic_error("Empty resolve action!");

    // ref to not invalidate all ptrs
    auto &us = Settings::get_user_settings();
    auto cr = us.remotes.begin();
    current_remote = &*cr++;

    auto resolve_remote_deps = [this, &deps, &cr, &us]()
    {
        bool again = true;
        while (again)
        {
            again = false;
            try
            {
                if (us.remotes.size() > 1)
                    LOG_INFO(logger, "Trying " + current_remote->name + " remote");
                download_dependencies_ = getDependenciesFromRemote(deps, current_remote);
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

    query_local_db = !us.force_server_query;
    // do 2 attempts: 1) local db, 2) remote db
    int n_attempts = query_local_db ? 2 : 1;
    while (n_attempts--)
    {
        try
        {
            if (query_local_db)
            {
                try
                {
                    download_dependencies_ = getDependenciesFromDb(deps, current_remote);
                }
                catch (std::exception &e)
                {
                    LOG_ERROR(logger, "Cannot get dependencies from local database: " << e.what());

                    query_local_db = false;
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

            query_local_db = false;
            continue;
        }
        break;
    }
}

void Resolver::download(const ExtendedPackageData &d, const path &fn)
{
    if (!d.remote->downloadPackage(d, d.hash, fn, query_local_db))
    {
        // if we get hashes from local db
        // they can be stalled within server refresh time (15 mins)
        // in this case we should do request to server
        auto err = "Hashes do not match for package: " + d.target_name;
        if (query_local_db)
            throw LocalDbHashException(err);
        throw std::runtime_error(err);
    }
}

void Resolver::download_and_unpack()
{
    if (download_dependencies_.empty())
        return;

    auto download_dependency = [this](auto &dd)
    {
        auto &d = dd.second;
        auto version_dir = d.getDirSrc();
        auto hash_file = d.getStampFilename();
        bool must_download = d.getStampHash() != d.hash || d.hash.empty();

        auto &sdb = getServiceDatabase();

        if (fs::exists(version_dir) && !must_download && sdb.isPackageInstalled(d) != 0)
            return;

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
        LOG_INFO(logger, "Downloading: " << d.target_name << "...");

        // maybe d.target_name instead of version_dir.string()?
        path fn = make_archive_name((temp_directory_path("dl") / d.target_name).string());
        download(d, fn);

        // verify before cleaning old pkg
        //if (Settings::get_local_settings().verify_all)
        //    verify(d, fn);

        // remove existing version dir
        //cleanPackages(d.target_name);

        //rd.downloads++;
        write_file(hash_file, d.hash);

        // remove all old possible created files, not more needed files etc.
        fs::remove_all(d.getDir());

        LOG_INFO(logger, "Unpacking  : " << d.target_name << "...");
        Files files;
        try
        {
            files = unpack_file(fn, version_dir);
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
        create_link(d.getDirSrc().parent_path(), getUserDirectories().storage_dir_lnk / "src" / (d.target_name + ".lnk"));
        //create_link(d.getDirObj(), directories.storage_dir_lnk / "obj" / (cc.first.target_name + ".lnk"));
#endif

        // cppan2 case
        if (auto files = enumerate_files(version_dir, false); files.size() == 1 && files.begin()->filename().string() == "sw.cpp")
            return;
        // another workaround
        error_code ec;
        if (fs::exists(version_dir / SW_SDIR_NAME / "sw.cpp", ec))
            return;

        // re-read in any case
        auto root = YAML::LoadFile((version_dir / "cppan.yml").string());
        if (root["unpack_directory"].IsDefined())
        {
            path ud = root["unpack_directory"].template as<String>();
            ud = version_dir / ud;
            if (fs::exists(ud))
                throw std::runtime_error("Cannot create unpack_directory '" + ud.string() + "' because fs object with the same name alreasy exists");
            fs::create_directories(ud);
            for (auto &f : boost::make_iterator_range(fs::directory_iterator(version_dir), {}))
            {
                if (f == ud || f.path().filename() == CPPAN_FILENAME)
                    continue;
                if (fs::is_directory(f))
                {
                    copy_dir(f, ud / f.path().filename());
                    fs::remove_all(f);
                }
                else if (fs::is_regular_file(f))
                {
                    fs::copy_file(f, ud / f.path().filename());
                    fs::remove(f);
                }
            }
        }
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
    if (query_local_db)
    {
        // send download list
        // remove this when cppan will be widely used
        // also because this download count can be easily abused
        e.push([this]()
        {
            if (!current_remote)
                return;

            ptree request;
            ptree children;
            for (auto &d : download_dependencies_)
            {
                ptree c;
                c.put("", d.second.id);
                children.push_back(std::make_pair("", c));
            }
            request.add_child("vids", children);

            try
            {
                HttpRequest req = httpSettings;
                req.type = HttpRequest::Post;
                req.url = current_remote->url + "/api/add_downloads";
                req.data = ptree2string(request);
                auto resp = url_request(req);
            }
            catch (...)
            {
            }
        });
    }

    // send download action once
    RUN_ONCE
    {
        e.push([this]
        {
            try
            {
                HttpRequest req = httpSettings;
                req.type = HttpRequest::Post;
                req.url = current_remote->url + "/api/add_client_call";
                req.data = "{}"; // empty json
                auto resp = url_request(req);
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
    // prepare request
    ptree request;
    ptree dependency_tree;
    for (auto &d : deps)
    {
        ptree version;
        version.put("version", d.ppath.is_pvt() ? d.range.toStringV1() : d.range.toString());
        request.add_child(ptree::path_type(d.ppath.toString(), '|'), version);
    }

    LOG_INFO(logger, "Requesting dependency list... ");
    {
        int ct = 5;
        int t = 10;
        int n_tries = 3;
        while (1)
        {
            HttpResponse resp;
            try
            {
                HttpRequest req = httpSettings;
                req.connect_timeout = ct;
                req.timeout = t;
                req.type = HttpRequest::Post;
                req.url = current_remote->url;
                if (req.url.back() != '/')
                    req.url += '/';
                req.url += "api/find_dependencies";
                req.content_type = "application/json";
                req.data = ptree2string(request);
                resp = url_request(req);
                if (resp.http_code != 200)
                    throw std::runtime_error("Cannot get deps");
                dependency_tree = string2ptree(resp.response);
                break;
            }
            catch (...)
            {
                if (--n_tries == 0)
                {
                    switch (resp.http_code)
                    {
                    case 200:
                    {
                        dependency_tree = string2ptree(resp.response);
                        auto e = dependency_tree.find("error");
                        LOG_WARN(logger, e->second.get_value<String>());
                    }
                    break;
                    case 0:
                        LOG_WARN(logger, "Could not connect to server");
                        break;
                    default:
                        LOG_WARN(logger, "Error code: " + std::to_string(resp.http_code));
                        break;
                    }
                    throw;
                }
                else if (resp.http_code == 0)
                {
                    ct /= 2;
                    t /= 2;
                }
                LOG_INFO(logger, "Retrying... ");
            }
        }
    }

    // read deps urls, download them, unpack
    int api = 0;
    if (dependency_tree.find("api") != dependency_tree.not_found())
        api = dependency_tree.get<int>("api");

    auto e = dependency_tree.find("error");
    if (e != dependency_tree.not_found())
        throw std::runtime_error(e->second.get_value<String>());

    auto info = dependency_tree.find("info");
    if (info != dependency_tree.not_found())
        LOG_INFO(logger, info->second.get_value<String>());

    if (api == 0)
        throw std::runtime_error("API version is missing in the response");
    if (api > CURRENT_API_LEVEL)
        throw std::runtime_error("Server uses more new API version. Please, upgrade the cppan client from site or via --self-upgrade");
    if (api < CURRENT_API_LEVEL - 1)
        throw std::runtime_error("Your client's API is newer than server's. Please, wait for server upgrade");

    // dependencies were received without error

    // we might get less packages than requested if we request same packages, so detect duplicates
    std::unordered_map<PackagePath, int> dups;
    for (auto &d : deps)
        dups[d.ppath]++;

    // set id dependencies
    IdDependencies id_deps;
    int unresolved = (int)deps.size();
    auto &remote_packages = dependency_tree.get_child("packages");
    for (auto &v : remote_packages)
    {
        auto id = v.second.get<db::PackageVersionId>("id");

        DownloadDependency d;
        d.ppath = v.first;
        d.version = v.second.get<String>("version");
        d.flags = decltype(d.flags)(v.second.get<uint64_t>("flags"));
        // TODO: remove later sha256 field
        d.hash = v.second.get<String>("sha256", "empty_hash");
        if (d.hash == "empty_hash")
            d.hash = v.second.get<String>("hash", "empty_hash");
        d.group_number = v.second.get<PackageVersionGroupNumber>("gn", 0);

        if (v.second.find(DEPENDENCIES_NODE) != v.second.not_found())
        {
            std::unordered_set<db::PackageVersionId> idx;
            for (auto &tree_dep : v.second.get_child(DEPENDENCIES_NODE))
                idx.insert(tree_dep.second.get_value<db::PackageVersionId>());
            d.setDependencyIds(idx);
        }

        id_deps[id] = d;

        unresolved -= dups[d.ppath];
        dups[d.ppath] = 0;
    }

    if (unresolved > 0)
    {
        String pkgs;
        for (auto &[d, n] : dups)
        {
            if (n == 0)
                continue;
            pkgs += d.toString() + ", ";
        }
        if (!pkgs.empty())
            pkgs.resize(pkgs.size() - 2);
        throw std::runtime_error("Some packages (" + std::to_string(unresolved) + ") are unresolved: " + pkgs);
    }

    return prepareIdDependencies(id_deps, current_remote);
}

Resolver::Dependencies getDependenciesFromDb(const UnresolvedPackages &deps, const Remote *current_remote)
{
    auto &db = getPackagesDatabase();
    auto id_deps = db.findDependencies(deps);
    return prepareIdDependencies(id_deps, current_remote);
}

Resolver::Dependencies prepareIdDependencies(const IdDependencies &id_deps, const Remote *current_remote)
{
    Resolver::Dependencies dependencies;
    for (auto &v : id_deps)
    {
        auto d = v.second;
        d.createNames();
        d.remote = current_remote;
        d.prepareDependencies(id_deps);
        dependencies[d] = d;
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
    ResolverPackagesMap pkgs2;
    try
    {
        pkgs2 = resolve_dependencies({ p });
    }
    catch (const std::runtime_error &)
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
