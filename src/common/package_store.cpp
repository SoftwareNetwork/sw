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

#include "package_store.h"

#include "access_table.h"
#include "config.h"
#include "database.h"
#include "directories.h"
#include "exceptions.h"
#include "hash.h"
#include "lock.h"
#include "project.h"
#include "resolver.h"
#include "settings.h"
#include "sqlite_database.h"

#include <boost/algorithm/string.hpp>

#include <primitives/executor.h>
#include <primitives/hasher.h>
#include <primitives/http.h>
#include <primitives/templates.h>
#include <primitives/win32helpers.h>

#include <primitives/log.h>
//DECLARE_STATIC_LOGGER(logger, "package_store");

// legacy varname rd - was: response data
PackageStore rd;

Strings extract_comments(const String &s);

void download_file(path &fn)
{
    // this function checks if fn is url,
    // tries to download it to current dir and run cppan on it
    auto s = fn.string();
    if (!isUrl(s))
        return;
    fn = fn.filename();
    download_file(s, fn, 1_GB);
}

void PackageStore::process(const path &p, Config &root)
{
    if (processing)
        return;
    processing = true;

    SCOPE_EXIT
    {
        processing = false;
    };

    // main access table holder
    AccessTable access_table;

    // insert root config
    packages[root.pkg].config = &root;

    // resolve deps
    for (auto &c : packages)
    {
        if (!c.second.config)
            throw std::runtime_error("Config was not created for target: " + c.first.target_name);

        resolve_dependencies(*c.second.config);
    }

    // set correct local package flags to rd[d].dependencies
    for (auto &c : packages)
    {
        if (!c.first.flags[pfLocalProject])
            continue;
        // only for local packages!!!
        for (auto &d : c.second.dependencies)
        {
            // i->first equals to d.second but have correct flags!!!
            // so we assign them to d.second
            auto i = packages.find(d.second);
            if (i == packages.end())
            {
                // for pretty error message
                auto dep = d.second;
                dep.createNames();

                // we try to resolve again
                ::resolve_dependencies({ d });

                auto irp = resolved_packages.find(d.second);
                if (irp == resolved_packages.end())
                    throw std::runtime_error(c.first.target_name + ": cannot find match for " + dep.target_name);

                i = packages.find(irp->second);
                if (i == packages.end())
                {
                    throw std::logic_error("resolved package does not exist in packages var! " +
                        c.first.target_name + ": cannot find match for " + dep.target_name);
                }
            }
            if (!d.second.ppath.is_loc())
                continue;
            bool ido = d.second.flags[pfIncludeDirectoriesOnly] | i->first.flags[pfIncludeDirectoriesOnly];
            bool pvt = d.second.flags[pfPrivateDependency] | i->first.flags[pfPrivateDependency];
            d.second.flags = i->first.flags;
            d.second.flags.set(pfIncludeDirectoriesOnly, ido);
            d.second.flags.set(pfPrivateDependency, pvt);
        }
    }

    // TODO: if we got a download we might need to refresh configs
    // but we do not know what projects we should clear
    // so clear the whole AT
    if (rebuild_configs())
        access_table.clear();

    // gather (merge) checks, options etc.
    // add more necessary actions here
    for (auto &cc : *this)
    {
        if (cc.first == Package())
            continue;
        root.getDefaultProject().checks += cc.second.config->getDefaultProject().checks;
    }

    // make sure we have new printer every time

    // print deps
    // do not multithread this! causes livelocks
    for (auto &cc : *this)
    {
        if (cc.first == Package())
            continue;
        auto &d = cc.first;

        auto printer = Printer::create(Settings::get_local_settings().printerType);
        printer->access_table = &access_table;
        printer->d = d;
        printer->cwd = d.getDirObj();
        printer->print();
        printer->print_meta();
    }

    // have some influence on printer->print_meta();
    // do not remove
    ScopedCurrentPath cp(p, CurrentPathScope::All);

    // print root config
    auto printer = Printer::create(Settings::get_local_settings().printerType);
    printer->access_table = &access_table;
    printer->d = root.pkg;
    printer->cwd = cp.get_cwd();
    printer->print_meta();
}

void PackageStore::resolve_dependencies(const Config &c)
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
            packages[c.pkg].dependencies.insert({ i->second.ppath.toString(), i->second });
            continue;
        }

        deps.insert(d);
    }

    if (deps.empty())
        return;

    Resolver r;
    r.resolve_dependencies(deps);
    r.assign_dependencies(c.pkg, deps);

    write_index();
    check_deps_changed(); // goes after write_index()
}

void PackageStore::check_deps_changed()
{
    // already executed
    if (deps_changed)
        return;

    // deps are now resolved
    // now refresh dependencies database only for remote packages
    // this file (local,current,root) packages will be refreshed anyway
    auto &sdb = getServiceDatabase();
    std::unordered_map<Package, String> clean_pkgs;
    for (auto &cc : *this)
    {
        if (cc.first == Package())
            continue;
        // make sure we have ordered deps
        Hasher h;
        StringSet deps;
        for (auto &d : cc.second.dependencies)
            deps.insert(d.second.target_name);
        for (auto &d : deps)
            h |= d;

        if (!sdb.hasPackageDependenciesHash(cc.first, h.hash))
        {
            deps_changed = true;

            // clear exports for this project, so it will be regenerated
            auto p = Printer::create(Settings::get_local_settings().printerType);
            p->clear_export(cc.first.getDirObj());
            clean_pkgs.emplace(cc.first, h.hash);
        }
    }

    auto &e = getExecutor();
    std::vector<Future<void>> fs;
    for (auto &kv : clean_pkgs)
    {
        fs.push_back(e.push([&kv, &sdb]
        {
            cleanPackages(kv.first.target_name, CleanTarget::Lib | CleanTarget::Bin | CleanTarget::Obj | CleanTarget::Exp);
            // set dep hash only after clean
            sdb.setPackageDependenciesHash(kv.first, kv.second);
        }));
    }
    for (auto &f : fs)
        f.wait();
    for (auto &f : fs)
        f.get();
}

PackageStore::iterator PackageStore::begin()
{
    return packages.begin();
}

PackageStore::iterator PackageStore::end()
{
    return packages.end();
}

PackageStore::const_iterator PackageStore::begin() const
{
    return packages.begin();
}

PackageStore::const_iterator PackageStore::end() const
{
    return packages.end();
}

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

void PackageStore::write_index() const
{
#ifdef _WIN32
    auto create_link = [](const auto &p, const auto &ln)
    {
        if (!fs::exists(ln))
            ::create_link(p, ln, "CPPAN link");
    };
#endif

    auto &sdb = getServiceDatabase();
    for (auto &cc : *this)
    {
        if (cc.first == Package())
            continue;
        sdb.addInstalledPackage(cc.first);
#ifdef _WIN32
        create_link(cc.first.getDirSrc(), directories.storage_dir_lnk / "src" / (cc.first.target_name + ".lnk"));
        create_link(cc.first.getDirObj(), directories.storage_dir_lnk / "obj" / (cc.first.target_name + ".lnk"));
#endif
    }
}

Config *PackageStore::add_config(std::unique_ptr<Config> &&config, bool created)
{
    auto cfg = config.get();
    auto i = config_store.insert(std::move(config));
    packages[cfg->pkg].config = i.first->get();
    packages[cfg->pkg].config->created = created;
    return packages[cfg->pkg].config;
}

Config *PackageStore::add_config(const Package &p, bool local)
{
    auto c = std::make_unique<Config>(p.getDirSrc(), local);
    c->setPackage(p);
    return add_config(std::move(c), true);
}

Config *PackageStore::add_local_config(const Config &co)
{
    auto cu = std::make_unique<Config>(co);
    auto cp = add_config(std::move(cu), true);
    resolve_dependencies(*cp);
    return cp;
}

std::tuple<PackagesSet, Config, String>
PackageStore::read_packages_from_file(path p, const String &config_name, bool direct_dependency)
{
    download_file(p);
    p = fs::canonical(fs::absolute(p));

    Config conf;
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
                auto root = load_yaml_config(comments[i]);

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
            auto root = load_yaml_config(comments[load_ok.front()]);
            conf.load(root);
        }
    };

    auto build_spec_file = [](const path &p)
    {
        Config c;

        // allow defaults for spec file
        c.defaults_allowed = true;

        // allow relative project names
        c.allow_relative_project_names = true;

        c.reload(p);
        return c;
    };

    auto set_config = [&config_name](const auto &fn)
    {
        auto root = load_yaml_config(fn);
        root["local_settings"]["current_build"] = config_name;
        Settings::get_local_settings().load(root["local_settings"], SettingsType::Local);
    };

    String sname;
    path cpp_fn;
    if (fs::is_regular_file(p))
    {
        if (p.filename() == CPPAN_FILENAME)
        {
            conf = build_spec_file(p.parent_path());
            sname = p.parent_path().filename().string();
            set_config(p);
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
        ScopedCurrentPath cp(p, CurrentPathScope::All);

        auto cppan_fn = p / CPPAN_FILENAME;
        auto main_fn = p / "main.cpp";
        if (fs::exists(cppan_fn))
        {
            conf = build_spec_file(cppan_fn.parent_path());
            sname = cppan_fn.parent_path().filename().string();
            set_config(cppan_fn);
            p = cppan_fn;
        }
        else
        {
            if (!fs::exists(main_fn) && fs::exists("main.c"))
                main_fn = "main.c";

            if (fs::exists(main_fn))
            {
                read_from_cpp(main_fn);
                p = main_fn;
                sname = p.filename().stem().string();
                cpp_fn = p;
            }
            else
            {
                LOG_DEBUG(logger, "No candidates {cppan.yml|main.c[pp]} for reading in directory " + p.string() +
                    ". Assuming default config.");

                conf = build_spec_file(p);
                sname = p.filename().string();
            }
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

    PackagesSet packages;
    auto configs = conf.split();

    // batch resolve of deps first; merge flags?

    // seq
    for (auto &c : configs)
    {
        auto &project = c.getDefaultProject();
        auto root_directory = (fs::is_regular_file(p) ? p.parent_path() : p) / project.root_directory;

        // to prevent possible errors
        // pkg must have small scope
        Package pkg;
        pkg.ppath = ppath;
        if (!project.name.empty())
        {
            //pkg.ppath.push_back(project.name);
            pkg.ppath /= ProjectPath(project.name);
        }
        pkg.version = Version(LOCAL_VERSION_NAME);
        pkg.flags.set(pfLocalProject);
        pkg.flags.set(pfDirectDependency, direct_dependency);
        pkg.createNames();
        project.applyFlags(pkg.flags);
        c.setPackage(pkg);
        local_packages[pkg.ppath] = root_directory;
    }

    Executor e(std::thread::hardware_concurrency() * 2);
    std::vector<Future<void>> fs;
    for (auto &c : configs)
    {
        auto f = e.push([&c, &p, &cpp_fn, &ppath]()
        {
            auto &project = c.getDefaultProject();
            auto root_directory = fs::is_regular_file(p) ? p.parent_path() : p;
            if (project.root_directory.is_absolute())
                root_directory = project.root_directory;
            else
                root_directory /= project.root_directory;

            // sources
            if (!cpp_fn.empty() && !project.files_loaded)
            {
                // clear default sources first
                project.sources.clear();
                project.sources.insert(cpp_fn.filename().string());
            }
            project.root_directory = root_directory;
            LOG_INFO(logger, "Finding sources for " + project.pkg.ppath.slice(2).toString());
            project.findSources(root_directory);
            // maybe remove? let user see cppan.yml in local project
            project.files.erase(current_thread_path() / CPPAN_FILENAME);
            project.files.erase(CPPAN_FILENAME);
            // patch if any
            project.patchSources();

            // update flags and pkg again after findSources()
            // project type may be different
            // at this time we take project.pkg, not just local variable (pkg)
            project.applyFlags(project.pkg.flags);
            c.setPackage(project.pkg);

            if (Settings::get_local_settings().install_local_packages)
            {
                // copy files to project's dir
                decltype(project.files) files;
                for (auto &f : project.files)
                {
                    auto r = project.pkg.getDirSrc() / f.lexically_relative(root_directory);
                    create_directories(r.parent_path());
                    fs::copy_file(f, r, fs::copy_option::overwrite_if_exists);
                    files.insert(r);
                }
                project.files = files;

                // set non local
                //project.pkg.flags.set(pfLocalProject, false);
            }

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
        });
        fs.push_back(f);
    }
    for (auto &f : fs)
        f.wait();
    for (auto &f : fs)
        f.get();

    // seq
    for (auto &c : configs)
    {
        auto &project = c.getDefaultProject();

        // add package for result
        packages.insert(project.pkg);

        // add config to storage
        rd.add_local_config(c);
    }

    // write local packages to index
    // do not remove
    rd.write_index();

    return std::tuple<PackagesSet, Config, String>{ packages, conf, sname };
}

bool PackageStore::has_local_package(const ProjectPath &ppath) const
{
    return local_packages.find(ppath) != local_packages.end();
}

path PackageStore::get_local_package_dir(const ProjectPath &ppath) const
{
    auto i = local_packages.find(ppath);
    if (i != local_packages.end())
        return i->second;
    return path();
}
