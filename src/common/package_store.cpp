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

#include "package_store.h"

#include "access_table.h"
#include "config.h"
#include "database.h"
#include "directories.h"
#include "exceptions.h"
#include "executor.h"
#include "hash.h"
#include "hasher.h"
#include "http.h"
#include "lock.h"
#include "project.h"
#include "resolver.h"
#include "settings.h"
#include "sqlite_database.h"
#include "templates.h"

#include <boost/algorithm/string.hpp>

#include "logger.h"
DECLARE_STATIC_LOGGER(logger, "package_store");

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

    DownloadData dd;
    dd.url = s;
    dd.file_size_limit = 1'000'000'000;
    dd.fn = fn;
    download_file(dd);
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
            bool ido = d.second.flags[pfIncludeDirectoriesOnly] | i->first.flags[pfIncludeDirectoriesOnly];
            bool pvt = d.second.flags[pfPrivateDependency] | i->first.flags[pfPrivateDependency];
            d.second.flags = i->first.flags;
            d.second.flags.set(pfIncludeDirectoriesOnly, ido);
            d.second.flags.set(pfPrivateDependency, pvt);
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
        root.getDefaultProject().checks += cc.second.config->getDefaultProject().checks;
    }

    auto printer = Printer::create(Settings::get_local_settings().printerType);
    printer->access_table = &access_table;

    // print deps
    for (auto &cc : *this)
    {
        auto &d = cc.first;

        printer->d = d;
        printer->cwd = d.getDirObj();
        printer->print();
        printer->print_meta();
    }

    ScopedCurrentPath cp(p);

    // print root config
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
    for (auto &cc : *this)
    {
        Hasher h;
        for (auto &d : cc.second.dependencies)
            h |= d.second.target_name;
        if (!sdb.hasPackageDependenciesHash(cc.first, h.hash))
        {
            deps_changed = true;

            // clear exports for this project, so it will be regenerated
            auto p = Printer::create(Settings::get_local_settings().printerType);
            p->clear_export(cc.first.getDirObj());
            cleanPackages(cc.first.target_name, CleanTarget::Lib | CleanTarget::Bin);
            sdb.setPackageDependenciesHash(cc.first, h.hash);
        }
    }
}

PackageStore::iterator PackageStore::begin()
{
    auto i = packages.find(Package());
    if (i != packages.end())
        return ++i;
    return packages.begin();
}

PackageStore::iterator PackageStore::end()
{
    return packages.end();
}

PackageStore::const_iterator PackageStore::begin() const
{
    auto i = packages.find(Package());
    if (i != packages.end())
        return ++i;
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
    auto &sdb = getServiceDatabase();
    for (auto &cc : *this)
        sdb.addInstalledPackage(cc.first);
}

Config *PackageStore::add_config(std::unique_ptr<Config> &&config, bool created)
{
    auto cfg = config.get();
    auto i = config_store.insert(std::move(config));
    packages[cfg->pkg].config = i.first->get();
    packages[cfg->pkg].config->created = created;
    return packages[cfg->pkg].config;
}

Config *PackageStore::add_config(const Package &p)
{
    auto c = std::make_unique<Config>(p.getDirSrc());
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

std::tuple<std::set<Package>, Config, String>
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
        auto root_directory = (fs::is_regular_file(p) ? p.parent_path() : p) / project.root_directory;

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
        local_packages[pkg.ppath] = root_directory;

        // sources
        if (!cpp_fn.empty() && !project.files_loaded)
        {
            // clear default sources first
            project.sources.clear();
            project.sources.insert(cpp_fn.filename().string());
        }
        project.root_directory = root_directory;
        project.findSources(root_directory);
        // maybe remove? let user see cppan.yml in local project
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
