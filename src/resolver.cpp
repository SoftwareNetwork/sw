/*
 * Copyright (c) 2016, Egor Pugin
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     1. Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *     3. Neither the name of the copyright holder nor the names of
 *        its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "resolver.h"

#include "access_table.h"
#include "config.h"
#include "database.h"
#include "directories.h"
#include "exceptions.h"
#include "executor.h"
#include "hash.h"
#include "hasher.h"
#include "lock.h"
#include "log.h"
#include "project.h"
#include "sqlite_database.h"
#include "templates.h"

#include <boost/algorithm/string.hpp>

#include "logger.h"
DECLARE_STATIC_LOGGER(logger, "response");

#define CURRENT_API_LEVEL 1

TYPED_EXCEPTION(LocalDbHashException);

// legacy varname rd
// was: response data
Resolver rd;

Executor &getExecutor()
{
    static Executor executor(2);
    return executor;
}

void Resolver::process(const path &p, Config &root)
{
    if (processing)
        return;
    processing = true;

    SCOPE_EXIT
    {
        processing = false;
    };

    // insert root config
    packages[root.pkg].config = &root;

    // resolve deps
    for (auto &c : packages)
    {
        if (!c.second.config)
            throw std::runtime_error("Config was not created for target: " + c.first.target_name);

        resolve_dependencies(*c.second.config);
    }

    // set correct package flags to rd[d].dependencies
    for (auto &c : packages)
    {
        for (auto &d : c.second.dependencies)
        {
            // i->first equals to d.second but have correct flags!!!
            // so we assign them to d.second
            auto i = packages.find(d.second);
            if (i == packages.end())
                throw std::runtime_error("Cannot find match for " + d.second.target_name);
            d.second.flags = i->first.flags;
        }
    }

    // main access table holder
    AccessTable access_table(directories.storage_dir_etc);

    // TODO: if we got a download we might need to refresh configs
    // but we do not know what projects we should clear
    // so clear the whole AT
    if (rebuild_configs())
        access_table.clear();

    // gather (merge) checks, options etc.
    // add more necessary actions here
    for (auto &cc : *this)
    {
        auto &d = cc.first;
        auto c = cc.second.config;

        root.checks += c->checks;

        const auto &p = c->getDefaultProject();
        for (auto &ol : p.options)
        {
            if (!ol.second.global_definitions.empty())
                c->global_options[ol.first].global_definitions.insert(
                    ol.second.global_definitions.begin(), ol.second.global_definitions.end());
        }
    }

    auto printer = Printer::create(root.settings.printerType);
    printer->access_table = &access_table;
    printer->rc = &root;

    // print deps
    for (auto &cc : *this)
    {
        auto &d = cc.first;
        auto c = cc.second.config;

        printer->d = d;
        printer->cc = c;
        printer->cwd = d.getDirObj();

        printer->print();
        printer->print_meta();
    }

    ScopedCurrentPath cp(p);

    // print root config
    printer->cc = &root;
    printer->d = Package();
    printer->cwd = cp.get_cwd();
    printer->print_meta();
}

void Resolver::resolve_dependencies(const Config &c)
{
    if (c.getProjects().size() > 1)
        throw std::runtime_error("Make sure your config has only one project (call split())");

    if (!packages[c.pkg].dependencies.empty())
        return;

    Packages deps;

    // remove some packages
    for (auto &d : c.getFileDependencies())
    {
        // remove local packages
        if (d.second.ppath.is_loc())
        {
            // but still insert as a dependency
            packages[c.pkg].dependencies.insert(d);
            continue;
        }

        // remove already downloaded packages
        auto i = resolved_packages.find(d.second);
        if (i != resolved_packages.end())
        {
            // but still insert as a dependency
            packages[c.pkg].dependencies.insert({ i->ppath.toString(), *i });
            continue;
        }

        deps.insert(d);
    }

    if (deps.empty())
        return;

    resolve_dependencies(deps);
    read_configs();
    post_download();
    write_index();
    check_deps_changed();

    // add input config
    packages[c.pkg].dependencies = deps;
    for (auto &dd : download_dependencies_)
    {
        if (!dd.second.flags[pfDirectDependency])
            continue;
        auto &deps2 = packages[c.pkg].dependencies;
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
        auto i = resolved_packages.find(d.second);
        if (i != resolved_packages.end())
            continue;

        deps.insert(d);
    }

    if (deps.empty())
        return;

    auto uc = Config::get_user_config();
    auto cr = uc.settings.remotes.begin();
    current_remote = &*cr++;

    auto resolve_remote_deps = [this, &deps, &cr, &uc]()
    {
        bool again = true;
        while (again)
        {
            again = false;
            try
            {
                LOG_INFO(logger, "Trying " + current_remote->name + " remote");
                getDependenciesFromRemote(deps);
            }
            catch (const std::exception &e)
            {
                if (cr != uc.settings.remotes.end())
                {
                    LOG(e.what());
                    current_remote = &*cr++;
                    again = true;
                }
                else
                    throw;
            }
        }
    };

    query_local_db = !uc.settings.force_server_query;
    // do 2 attempts: 1) local db, 2) remote db
    int n_attempts = query_local_db ? 2 : 1;
    while (n_attempts--)
    {
        // clear before proceed
        download_dependencies_.clear();

        try
        {
            if (query_local_db)
            {
                try
                {
                    getDependenciesFromDb(deps);
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

            download_and_unpack();
        }
        catch (LocalDbHashException &)
        {
            LOG_WARN(logger, "Local db data caused issues, trying remote one");

            query_local_db = false;
            continue;
        }
        break;
    }

    // mark packages as resolved
    for (auto &d : deps)
        resolved_packages.insert(d.second);
}

void Resolver::check_deps_changed()
{
    // already executed
    if (deps_changed)
        return;

    // deps are now resolved
    // now refresh dependencies database only for remote packages
    // this file (local,current,root) packages will be refreshed anyway
    auto &sdb = getServiceDatabase();
    for (auto &cc : *this)
    {
        Hasher h;
        for (auto &d : cc.second.dependencies)
            h |= d.second.target_name;
        if (!sdb.hasPackageDependenciesHash(cc.first, h.hash))
        {
            deps_changed = true;

            // clear exports for this project, so it will be regenerated
            auto p = Printer::create(cc.second.config->settings.printerType);
            p->clear_export(cc.first.getDirObj());
            cleanPackages(cc.first.target_name, CleanTarget::Lib | CleanTarget::Bin);
            sdb.setPackageDependenciesHash(cc.first, h.hash);
        }
    }
}

void Resolver::getDependenciesFromRemote(const Packages &deps)
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

    LOG_NO_NEWLINE("Requesting dependency list... ");
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
                req.type = HttpRequest::POST;
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
                        LOG(e->second.get_value<String>());
                    }
                        break;
                    case 0:
                        LOG("Could not connect to server");
                        break;
                    default:
                        LOG("Error code: " + std::to_string(resp.http_code));
                        break;
                    }
                    throw;
                }
                else if (resp.http_code == 0)
                {
                    ct /= 2;
                    t /= 2;
                }
                LOG_NO_NEWLINE("Retrying... ");
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
        std::cout << info->second.get_value<String>() << "\n";

    if (api == 0)
        throw std::runtime_error("API version is missing in the response");
    if (api > CURRENT_API_LEVEL)
        throw std::runtime_error("Server uses more new API version. Please, upgrade the cppan client from site or via --self-upgrade");
    if (api < CURRENT_API_LEVEL - 1)
        throw std::runtime_error("Your client's API is newer than server's. Please, wait for server upgrade");

    // dependencies were received without error
    LOG("Ok");

    // set dependencies
    auto unresolved = deps.size();
    auto &remote_packages = dependency_tree.get_child("packages");
    for (auto &v : remote_packages)
    {
        auto id = v.second.get<ProjectVersionId>("id");

        DownloadDependency d;
        d.ppath = v.first;
        d.version = v.second.get<String>("version");
        d.flags = decltype(d.flags)(v.second.get<uint64_t>("flags"));
        d.sha256 = v.second.get<String>("sha256");
        d.createNames();
        dep_ids[d] = id;

        if (v.second.find(DEPENDENCIES_NODE) != v.second.not_found())
        {
            std::set<ProjectVersionId> idx;
            for (auto &tree_dep : v.second.get_child(DEPENDENCIES_NODE))
                idx.insert(tree_dep.second.get_value<ProjectVersionId>());
            d.setDependencyIds(idx);
        }

        d.map_ptr = &download_dependencies_;
        d.remote = current_remote;
        download_dependencies_[id] = d;

        unresolved--;
    }

    if (unresolved != 0)
        throw std::runtime_error("Some packages (" + std::to_string(unresolved) + ") are unresolved");
}

void Resolver::getDependenciesFromDb(const Packages &deps)
{
    auto &db = getPackagesDatabase();
    auto dl_deps = db.findDependencies(deps);

    // set dependencies
    for (auto &v : dl_deps)
    {
        auto &d = v.second;
        d.createNames();
        dep_ids[d] = d.id;
        d.map_ptr = &download_dependencies_;
        d.remote = current_remote;
        download_dependencies_[d.id] = d;
    }
}

void Resolver::download_and_unpack()
{
    auto download_dependency = [this](auto &dd)
    {
        auto &d = dd.second;
        auto version_dir = d.getDirSrc();
        auto hash_file = d.getStampFilename();

        // store hash of archive
        bool must_download = false;
        {
            std::ifstream ifile(hash_file.string());
            String hash;
            if (ifile)
            {
                ifile >> hash;
                ifile.close();
            }
            if (hash != d.sha256 || d.sha256.empty() || hash.empty())
                must_download = true;
        }

        if (fs::exists(version_dir) && !must_download)
            return;

        // lock, so only one cppan process at the time could download the project
        ScopedFileLock lck(hash_file, std::defer_lock);
        if (!lck.try_lock())
        {
            // wait & continue
            ScopedFileLock lck2(hash_file);
            add_config(d);
            return;
        }

        // remove existing version dir
        cleanPackages(d.target_name);

        auto fs_path = ProjectPath(d.ppath).toFileSystemPath().string();
        std::replace(fs_path.begin(), fs_path.end(), '\\', '/');
        String cppan_package_url = d.remote->url + "/" + d.remote->data_dir + "/" + fs_path + "/" + d.version.toString() + ".tar.gz";
        String github_package_url = "https://github.com/cppan-packages/" + d.getHash() + "/raw/master/" + make_archive_name();
        path fn = version_dir.string() + ".tar.gz";

        String dl_hash;
        DownloadData ddata;
        ddata.fn = fn;
        ddata.sha256.hash = &dl_hash;

        LOG_INFO(logger, "Downloading: " << d.target_name << "...");

        auto download_from_url = [this, &ddata, &dl_hash, &d](const auto &url, bool nothrow = true)
        {
            ddata.url = url;
            try
            {
                download_file(ddata);
            }
            catch (...)
            {
                if (nothrow)
                    return false;
                throw;
            }

            if (dl_hash != d.sha256)
            {
                if (nothrow)
                    return false;

                // if we get hashes from local db
                // they can be stalled within server refresh time (15 mins)
                // in this case we should do request to server
                if (query_local_db)
                    throw LocalDbHashException("Hashes do not match for package: " + d.target_name);
                throw std::runtime_error("Hashes do not match for package: " + d.target_name);
            }

            return true;
        };

        // at first we try to download from github
        // if we failed,try from cppan (this should be removed)
        if (!download_from_url(github_package_url, !query_local_db))
        {
            //LOG_ERROR(logger, "Fallback to cppan.org");
            download_from_url(cppan_package_url, false);
        }

        downloads++;
        write_file(hash_file, d.sha256);

        LOG_INFO(logger, "Unpacking  : " << d.target_name << "...");
        Files files;
        try
        {
            files = unpack_file(fn, version_dir);
        }
        catch (...)
        {
            fs::remove_all(version_dir);
            throw;
        }
        fs::remove(fn);

        // re-read in any case
        // no need to remove old config, let it die with program
        auto c = add_config(d);

        // move all files under unpack dir
        auto ud = c->getDefaultProject().unpack_directory;
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

    Executor e(get_max_threads(8));
    e.throw_exceptions = true;

    // threaded execution does not preserve object creation/destruction order,
    // so current path is not correctly restored
    ScopedCurrentPath cp;

    for (auto &dd : download_dependencies_)
        e.push([&download_dependency, &dd] { download_dependency(dd); });

    e.wait();

    if (query_local_db)
    {
        // send download list
        // remove this when cppan will be widely used
        // also because this download count can be easily abused
        getExecutor().push([this]()
        {
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
                req.type = HttpRequest::POST;
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
    static std::once_flag flag;
    std::call_once(flag, [this]
    {
        getExecutor().push([this]
        {
            try
            {
                HttpRequest req = httpSettings;
                req.type = HttpRequest::POST;
                req.url = current_remote->url + "/api/add_client_call";
                req.data = "{}"; // empty json
                auto resp = url_request(req);
            }
            catch (...)
            {
            }
        });
    });
}

void Resolver::post_download()
{
    for (auto &cc : *this)
        prepare_config(cc);
}

void Resolver::prepare_config(PackageConfigs::value_type &cc)
{
    auto &p = cc.first;
    auto &c = cc.second.config;
    auto &dependencies = cc.second.dependencies;
    c->setPackage(p);
    auto &project = c->getDefaultProject();

    if (p.flags[pfLocalProject])
        return;

    // prepare deps: extract real deps flags from configs
    for (auto &dep : download_dependencies_[dep_ids[p]].getDirectDependencies())
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
                for (auto &child_dep : download_dependencies_[dep_ids[p]].getDirectDependencies())
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

Resolver::PackageConfig &Resolver::operator[](const Package &p)
{
    return packages[p];
}

const Resolver::PackageConfig &Resolver::operator[](const Package &p) const
{
    auto i = packages.find(p);
    if (i == packages.end())
        throw std::runtime_error("Package not found: " + p.getTargetName());
    return i->second;
}

Resolver::iterator Resolver::begin()
{
    auto i = packages.find(Package());
    if (i != packages.end())
        return ++i;
    return packages.begin();
}

Resolver::iterator Resolver::end()
{
    return packages.end();
}

Resolver::const_iterator Resolver::begin() const
{
    auto i = packages.find(Package());
    if (i != packages.end())
        return ++i;
    return packages.begin();
}

Resolver::const_iterator Resolver::end() const
{
    return packages.end();
}

void Resolver::write_index() const
{
    auto &sdb = getServiceDatabase();
    for (auto &cc : *this)
        sdb.addInstalledPackage(cc.first);
}

void Resolver::read_configs()
{
    if (download_dependencies_.empty())
        return;
    LOG_NO_NEWLINE("Reading package specs... ");
    for (auto &d : download_dependencies_)
        read_config(d.second);
    LOG("Ok");
}

void Resolver::read_config(const DownloadDependency &d)
{
    if (!fs::exists(d.getDirSrc()))
        return;

    try
    {
        auto p = config_store.insert(std::make_unique<Config>(d.getDirSrc()));
        packages[d].config = p.first->get();
    }
    catch (std::exception &)
    {
        // something wrong, remove the whole dir to re-download it
        fs::remove_all(d.getDirSrc());
    }
}

Config *Resolver::add_config(std::unique_ptr<Config> &&config, bool created)
{
    auto cfg = config.get();
    auto i = config_store.insert(std::move(config));
    packages[cfg->pkg].config = i.first->get();
    packages[cfg->pkg].config->created = created;
    return packages[cfg->pkg].config;
}

Config *Resolver::add_config(const Package &p)
{
    auto c = std::make_unique<Config>(p.getDirSrc());
    c->setPackage(p);
    return add_config(std::move(c), true);
}

Config *Resolver::add_local_config(const Config &co)
{
    auto cu = std::make_unique<Config>(co);
    auto cp = add_config(std::move(cu), true);
    resolve_dependencies(*cp);
    return cp;
}

Strings extract_comments(const String &s);

void download_file(path &fn)
{
    // this function checks if fn is url,
    // tries to download it to current dir and run cppan on it
    auto s = fn.string();
    if (!isUrl(s))
        return;
    fn = fn.filename();

    DownloadData dd;
    dd.url = s;
    dd.file_size_limit = 1'000'000'000;
    dd.fn = fn;
    download_file(dd);
}

std::tuple<std::set<Package>, Config, String>
Resolver::read_packages_from_file(path p, const String &config_name, bool direct_dependency)
{
    download_file(p);
    p = fs::canonical(fs::absolute(p));

    auto conf = Config::get_user_config();
    conf.type = ConfigType::Local;
    conf.defaults_allowed = true; // was false
    conf.allow_local_dependencies = true;
    // allow relative project names
    conf.allow_relative_project_names = true;

    if (!fs::exists(p))
        throw std::runtime_error("File or directory does not exist: " + p.string());

    auto read_from_cpp = [&conf, &config_name](const path &fn)
    {
        auto s = read_file(fn);
        auto comments = extract_comments(s);

        std::vector<size_t> load_ok;
        bool found = false;
        for (size_t i = 0; i < comments.size(); i++)
        {
            bool probably_this = false;
            try
            {
                boost::trim(comments[i]);
                auto root = YAML::Load(comments[i]);

                auto sz = root.size();
                if (sz == 0)
                    continue;

                probably_this = root.IsMap() && (
                    root["local_settings"].IsDefined() ||
                    root["files"].IsDefined() ||
                    root["dependencies"].IsDefined()
                    );

                if (!config_name.empty())
                    root["local_settings"]["current_build"] = config_name;
                conf.load(root);

                if (probably_this)
                {
                    found = true;
                    break;
                }
                load_ok.push_back(i);
            }
            catch (...)
            {
                if (probably_this)
                    throw;
            }
        }

        // fallback to the first comment w/out error
        if (!found && !load_ok.empty())
        {
            auto root = YAML::Load(comments[load_ok.front()]);
            conf.load(root);
        }
    };

    auto build_spec_file = [](const path &p)
    {
        Config c(ConfigType::Local);

        // allow defaults for spec file
        c.defaults_allowed = true;

        // allow relative project names
        c.allow_relative_project_names = true;

        c.reload(p);
        return c;
    };

    String sname;
    path cpp_fn;
    if (fs::is_regular_file(p))
    {
        if (p.filename() == CPPAN_FILENAME)
        {
            conf = build_spec_file(p.parent_path());
            sname = p.parent_path().filename().string();
        }
        else
        {
            read_from_cpp(p);
            sname = p.filename().stem().string();
            cpp_fn = p;
        }
    }
    else if (fs::is_directory(p))
    {
        // config.load() will use proper defaults
        ScopedCurrentPath cp(p);

        auto cppan_fn = p / CPPAN_FILENAME;
        auto main_fn = p / "main.cpp";
        if (fs::exists(cppan_fn))
        {
            conf = build_spec_file(cppan_fn.parent_path());
            sname = cppan_fn.parent_path().filename().string();
            p = cppan_fn;
        }
        else if (fs::exists(main_fn))
        {
            read_from_cpp(main_fn);
            p = main_fn;
            sname = p.filename().stem().string();
            cpp_fn = p;
        }
        else
        {
            LOG_DEBUG(logger, "No candidates {cppan.yml|main.cpp} for reading in directory " + p.string() +
                ". Assuming default config.");

            conf = build_spec_file(p);
            sname = p.filename().string();
        }
    }
    else
        throw std::runtime_error("Unknown file type " + p.string());

    // prepare names
    auto pname = normalize_path(p);
#ifdef _WIN32 // || macos/ios?
    // prevent different project names for lower/upper case folders
    boost::to_lower(pname);
#endif

    for (auto &c : sname)
    {
        if (c < 0 || c > 127 || !isalnum(c))
            c = '_';
    }

    ProjectPath ppath;
    ppath.push_back("loc");
    ppath.push_back(sha256_short(pname));
    ppath.push_back(sname);

    // set package for root config
    {
        Package pkg;
        pkg.ppath = ppath;
        pkg.version = Version(LOCAL_VERSION_NAME);
        pkg.flags.set(pfLocalProject);
        pkg.flags.set(pfDirectDependency, direct_dependency);
        pkg.createNames();
        conf.setPackage(pkg);
    }

    std::set<Package> packages;
    auto configs = conf.split();
    // batch resolve of deps first; merge flags?
    for (auto &c : configs)
    {
        auto &project = c.getDefaultProject();

        Package pkg;
        pkg.ppath = ppath;
        if (!project.name.empty())
            pkg.ppath.push_back(project.name);
        pkg.version = Version(LOCAL_VERSION_NAME);
        pkg.flags.set(pfLocalProject);
        pkg.flags.set(pfDirectDependency, direct_dependency);
        pkg.createNames();
        project.applyFlags(pkg.flags);
        c.setPackage(pkg);
        local_packages.insert(pkg.ppath);

        // sources
        if (!cpp_fn.empty() && !project.files_loaded)
        {
            // clear default sources first
            project.sources.clear();
            project.sources.insert(cpp_fn.filename().string());
        }
        project.root_directory = (fs::is_regular_file(p) ? p.parent_path() : p) / project.root_directory;
        project.findSources(path());
        project.files.erase(CPPAN_FILENAME);

        // update flags and pkg again after findSources()
        // project type may be different
        // at this time we take project.pkg, not just local variable (pkg)
        project.applyFlags(project.pkg.flags);
        c.setPackage(project.pkg);

        // check if project's deps are relative
        // this means that there's a local dependency
        auto deps = project.dependencies;
        for (auto &d : deps)
        {
            if (!d.second.ppath.is_relative())
                continue;

            project.dependencies.erase(d.second.ppath.toString());

            d.second.ppath = ppath / d.second.ppath;
            d.second.version = Version(LOCAL_VERSION_NAME);
            d.second.createNames();
            project.dependencies.insert({ d.second.ppath.toString(), d.second });
        }

        // add config to storage
        rd.add_local_config(c);

        // add package for result
        packages.insert(pkg);
    }

    // write local packages to index
    // do not remove
    rd.write_index();

    return std::tuple<std::set<Package>, Config, String>{ packages, conf, sname };
}

bool Resolver::has_local_package(const ProjectPath &ppath) const
{
    return local_packages.find(ppath) != local_packages.end();
}
