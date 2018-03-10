/*
 * Copyright (C) 2016-2017, Egor Pugin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "resolver.h"

#include "access_table.h"
#include "config.h"
#include "database.h"
#include "directories.h"
#include "exceptions.h"
#include "lock.h"
#include "project.h"
#include "settings.h"
#include "sqlite_database.h"
#include "verifier.h"

#include <boost/algorithm/string.hpp>

#include <primitives/executor.h>
#include <primitives/hash.h>
#include <primitives/hasher.h>
#include <primitives/pack.h>
#include <primitives/templates.h>

#include <primitives/log.h>
//DECLARE_STATIC_LOGGER(logger, "resolver");

#define CURRENT_API_LEVEL 1

TYPED_EXCEPTION(LocalDbHashException);
TYPED_EXCEPTION(DependencyNotResolved);

Resolver::Dependencies getDependenciesFromRemote(const Packages &deps, const Remote *current_remote);
Resolver::Dependencies getDependenciesFromDb(const Packages &deps, const Remote *current_remote);
Resolver::Dependencies prepareIdDependencies(const IdDependencies &id_deps, const Remote *current_remote);

PackagesMap resolve_dependencies(const Packages &deps)
{
    Resolver r;
    r.resolve_dependencies(deps);
    return r.resolved_packages;
}

void resolve_and_download(const Package &p, const path &fn)
{
    Resolver r;
    r.resolve_and_download(p, fn);
}

void Resolver::resolve_dependencies(const Packages &dependencies)
{
    Packages deps;

    // remove some packages
    for (auto &d : dependencies)
    {
        // remove local packages
        if (d.second.ppath.is_loc())
            continue;

        // remove already downloaded packages
        auto i = rd.resolved_packages.find(d.second);
        if (i != rd.resolved_packages.end())
            continue;

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
            if (d.second.ppath == dl.second.ppath)
            {
                resolved_packages[d.second] = dl.second;
                continue;
            }
            // if this is not exact match, assign to self
            // TODO: or make resolved_packages multimap
            if (d.second.ppath.is_root_of(dl.second.ppath))
                resolved_packages[dl.second] = dl.second;
        }
    }
    // push to global
    rd.resolved_packages.insert(resolved_packages.begin(), resolved_packages.end());

    // other related stuff
    read_configs();
    post_download();
}

void Resolver::resolve_and_download(const Package &p, const path &fn)
{
    resolve({ { p.ppath.toString(), p } }, [&]
    {
        for (auto &dd : download_dependencies_)
        {
            if (dd.second == p)
            {
                download(dd.second, fn);
                break;
            }
        }
    });
}

void Resolver::resolve(const Packages &deps, std::function<void()> resolve_action)
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

        if (fs::exists(version_dir) && !must_download)
            return;

        // lock, so only one cppan process at the time could download the project
        ScopedFileLock lck(hash_file, std::defer_lock);
        if (!lck.try_lock())
        {
            // download is in progress, wait and register config
            ScopedFileLock lck2(hash_file);
            rd.add_config(d, false);
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
        if (Settings::get_local_settings().verify_all)
            verify(d, fn);

        // remove existing version dir
        cleanPackages(d.target_name);

        rd.downloads++;
        write_file(hash_file, d.hash);

        LOG_INFO(logger, "Unpacking  : " << d.target_name << "...");
        Files files;
        try
        {
            files = unpack_file(fn, version_dir);
        }
        catch (std::exception &e)
        {
            LOG_ERROR(logger, e.what());
            fs::remove(fn);
            fs::remove_all(version_dir);
            throw;
        }
        fs::remove(fn);

        // re-read in any case
        // no need to remove old config, let it die with program
        auto c = rd.add_config(d, false);

        // move all files under unpack dir
        auto ud = c->getDefaultProject(d.ppath).unpack_directory;
        if (!ud.empty())
        {
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

    Executor e(Settings::get_local_settings().max_download_threads, "Download thread");
    std::vector<Future<void>> fs;

    // threaded execution does not preserve object creation/destruction order,
    // so current path is not correctly restored
    // TODO: remove this! we must correctly run programs without this
    ScopedCurrentPath cp(CurrentPathScope::All);

    for (auto &dd : download_dependencies_)
        fs.push_back(e.push([&download_dependency, &dd] { download_dependency(dd); }));

    for (auto &f : fs)
        f.wait();
    for (auto &f : fs)
        f.get();

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

void Resolver::post_download()
{
    for (auto &cc : rd)
    {
        if (cc.first == Package())
            continue;
        prepare_config(cc);
    }
}

void Resolver::prepare_config(PackageStore::PackageConfigs::value_type &cc)
{
    auto &p = cc.first;
    auto &c = cc.second.config;
    auto &dependencies = cc.second.dependencies;
    c->setPackage(p);
    auto &project = c->getDefaultProject(p.ppath);

    if (p.flags[pfLocalProject])
        return;

    // prepare deps: extract real deps flags from configs
    for (auto &dep : download_dependencies_[p].dependencies)
    {
        auto d = dep.second;
        auto i = project.dependencies.find(d.ppath.toString());
        if (i == project.dependencies.end())
        {
            // check if we chose a root project that matches all subprojects
            Packages to_add;
            std::set<String> to_remove;
            for (auto &root_dep : project.dependencies)
            {
                for (auto &child_dep : download_dependencies_[p].dependencies)
                {
                    if (root_dep.second.ppath.is_root_of(child_dep.second.ppath))
                    {
                        to_add.insert({ child_dep.second.ppath.toString(), child_dep.second });
                        to_remove.insert(root_dep.second.ppath.toString());
                    }
                }
            }
            if (to_add.empty())
                throw std::runtime_error("dependency '" + d.ppath.toString() + "' not found");
            for (auto &r : to_remove)
                project.dependencies.erase(r);
            for (auto &a : to_add)
                project.dependencies.insert(a);
            continue;
        }
        d.flags[pfIncludeDirectoriesOnly] = i->second.flags[pfIncludeDirectoriesOnly];
        i->second.version = d.version;
        i->second.flags = d.flags;
        dependencies.emplace(d.ppath.toString(), d);
    }

    c->post_download();
}

void Resolver::read_configs()
{
    if (download_dependencies_.empty())
        return;
    LOG_INFO(logger, "Reading package specs... ");
    for (auto &d : download_dependencies_)
        read_config(d.second);
}

void Resolver::read_config(const ExtendedPackageData &d)
{
    if (!fs::exists(d.getDirSrc()))
    {
        LOG_DEBUG(logger, "Config dir does not exist: " << d.target_name);
        return;
    }

    if (rd.packages.find(d) != rd.packages.end())
    {
        LOG_DEBUG(logger, "Package does not exist: " << d.target_name);
        return;
    }

    // CPPAN_FILENAME must exist
    if (!fs::exists(d.getDirSrc() / CPPAN_FILENAME))
    {
        // if not - remove dir and fix everything on the next run
        fs::remove_all(d.getDirSrc());
        throw std::runtime_error("There is an error that cannot be resolved during this run, please, restart the program");
    }

    // keep some set data for re-read configs
    //auto oldi = rd.packages.find(d);
    // Config::created is needed for patching sources and other initialization stuff
    //bool created = oldi != rd.packages.end() && oldi->second.config->created;

    try
    {
        auto p = rd.config_store.insert(std::make_unique<Config>(d.getDirSrc(), false));
        /*auto ptr = */rd.packages[d].config = p.first->get();
        //ptr->created = created;
    }
    catch (DependencyNotResolved &)
    {
        // do not swallow
        throw;
    }
    catch (std::exception &)
    {
        // something wrong, remove the whole dir to re-download it
        fs::remove_all(d.getDirSrc());

        // but do not swallow
        throw;
    }
}

void Resolver::assign_dependencies(const Package &pkg, const Packages &deps)
{
    rd.packages[pkg].dependencies.insert(deps.begin(), deps.end());
    for (auto &dd : download_dependencies_)
    {
        if (!dd.second.flags[pfDirectDependency])
            continue;
        auto &deps2 = rd.packages[pkg].dependencies;
        auto i = deps2.find(dd.second.ppath.toString());
        if (i == deps2.end())
        {
            // check if we chose a root project match all subprojects
            Packages to_add;
            std::set<String> to_remove;
            for (auto &root_dep : deps2)
            {
                for (auto &child_dep : download_dependencies_)
                {
                    if (root_dep.second.ppath.is_root_of(child_dep.second.ppath))
                    {
                        to_add.insert({ child_dep.second.ppath.toString(), child_dep.second });
                        to_remove.insert(root_dep.second.ppath.toString());
                    }
                }
            }
            if (to_add.empty())
                throw std::runtime_error("cannot match dependency");
            for (auto &r : to_remove)
                deps2.erase(r);
            for (auto &a : to_add)
                deps2.insert(a);
            continue;
        }
        auto &d = i->second;
        d.version = dd.second.version;
        d.flags |= dd.second.flags;
        d.createNames();
    }
}

Resolver::Dependencies getDependenciesFromRemote(const Packages &deps, const Remote *current_remote)
{
    // prepare request
    ptree request;
    ptree dependency_tree;
    for (auto &d : deps)
    {
        ptree version;
        version.put("version", d.second.version.toAnyVersion());
        request.put_child(ptree::path_type(d.second.ppath.toString(), '|'), version);
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
                req.url = current_remote->url + "/api/find_dependencies";
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

    // set id dependencies
    IdDependencies id_deps;
    auto &remote_packages = dependency_tree.get_child("packages");
    for (auto &v : remote_packages)
    {
        auto id = v.second.get<ProjectVersionId>("id");

        DownloadDependency d;
        d.ppath = v.first;
        d.version = v.second.get<String>("version");
        d.flags = decltype(d.flags)(v.second.get<uint64_t>("flags"));
        // TODO: remove later sha256 field
        d.hash = v.second.get<String>("sha256", "empty_hash");
        if (d.hash == "empty_hash")
            d.hash = v.second.get<String>("hash", "empty_hash");

        if (v.second.find(DEPENDENCIES_NODE) != v.second.not_found())
        {
            std::unordered_set<ProjectVersionId> idx;
            for (auto &tree_dep : v.second.get_child(DEPENDENCIES_NODE))
                idx.insert(tree_dep.second.get_value<ProjectVersionId>());
            d.setDependencyIds(idx);
        }

        id_deps[id] = d;
    }

    // check resolved packages
    auto d2 = deps;
    for (auto &d : id_deps)
        d2.erase(d.second.ppath);
    if (!d2.empty())
    {
        // probably we have only root or dir dependency left
        // that is called from command line
        bool ok = false;
        if (d2.size() == 1 &&
            std::any_of(id_deps.begin(), id_deps.end(), [&d2](const auto &e) {
                return d2.begin()->second.ppath.is_root_of(e.second.ppath);
            }))
        {
            LOG_WARN(logger, "Skipping unresolved project: " + d2.begin()->second.target_name + ". Probably this is intended");
            ok = true;
        }

        if (!ok)
        {
            for (auto &d : d2)
            {
                d.second.createNames();
                LOG_FATAL(logger, "Unresolved package or its dependencies: " + d.second.target_name);
            }
            throw std::runtime_error("Some packages (" + std::to_string(d2.size()) + ") are unresolved");
        }
    }

    return prepareIdDependencies(id_deps, current_remote);
}

Resolver::Dependencies getDependenciesFromDb(const Packages &deps, const Remote *current_remote)
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

std::tuple<Package, PackagesSet> resolve_dependency(const String &target_name)
{
    String target = target_name;
    bool added_suffix = false;
    if (target.rfind('-') == target.npos)
    {
        target += "-*"; // add the latest version
        added_suffix = true;
    }
    auto p = extractFromString(target);
    PackagesSet pkgs;
    PackagesMap pkgs2;
    try
    {
        pkgs2 = resolve_dependencies({ { p.ppath.toString(), p } });
    }
    catch (const std::exception &)
    {
        if (!added_suffix)
            throw;

        target = target_name + "-master"; // add the master version
        p = extractFromString(target);
        pkgs2 = resolve_dependencies({ { p.ppath.toString(), p } });

        // TODO: if no master version, try to get first branch from local db
        // (another try ... catch)
        //target = target_name + "-master"; // add the master version
        //p = extractFromString(target);
        //resolved_deps = resolve_dependencies({ { p.ppath.toString(), p } });
    }
    for (auto &pkg : pkgs2)
        pkgs.insert(pkg.second);
    return std::make_tuple(p, pkgs);
}
