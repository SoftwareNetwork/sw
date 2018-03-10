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

#include "config.h"

#include "access_table.h"
#include "database.h"
#include "directories.h"
#include "lock.h"
#include "resolver.h"
#include "settings.h"
#include "yaml.h"

#include <boost/algorithm/string.hpp>

#include <primitives/hash.h>
#include <primitives/hasher.h>

#include <fstream>
#include <iostream>

#include <primitives/log.h>
//DECLARE_STATIC_LOGGER(logger, "config");

Config::Config()
{
    addDefaultProject();
	dir = current_thread_path();
}

Config::Config(const path &p, bool local)
    : Config()
{
    is_local = local;
    reload(p);
}

void Config::reload(const path &p)
{
    if (fs::is_directory(p))
    {
        dir = p;
        ScopedCurrentPath cp(dir, CurrentPathScope::Thread);
        load_current_config();
    }
    else
    {
        dir = p.parent_path();
		ScopedCurrentPath cp(dir, CurrentPathScope::Thread);
        load(p);
    }
}

void Config::addDefaultProject()
{
    Project p{ ProjectPath() };
	p.load(yaml());
    p.pkg = pkg;
    projects.clear();
    projects.emplace("", p);
}

void Config::load_current_config()
{
    if (fs::exists(dir / CPPAN_FILENAME))
        load(dir / CPPAN_FILENAME);
    else
        addDefaultProject();
}

void Config::load_current_config_settings()
{
    if (!fs::exists(dir / CPPAN_FILENAME))
        return addDefaultProject();
    auto root = load_yaml_config(dir / CPPAN_FILENAME);
    load_settings(root, false);
}

void Config::load(const path &p)
{
    auto root = load_yaml_config(p);
    load(root);
}

void Config::load(const String &s)
{
    auto root = load_yaml_config(s);
    load(root);
}

void Config::load_settings(const yaml &root, bool load_project)
{
    if (!check_config_root(root))
        return;

    auto ls = root["local_settings"];
    if (ls.IsDefined())
    {
        if (!ls.IsMap())
            throw std::runtime_error("'local_settings' should be a map");
        auto &ls = Settings::get_local_settings();
        ls.load_project = load_project;
        ls.load(root["local_settings"], SettingsType::Local);
        ls.load_project = true;
    }
}

bool Config::check_config_root(const yaml &root)
{
    if (root.IsNull() || !root.IsMap())
    {
        addDefaultProject();
        LOG_DEBUG(logger, "Spec file should be a map");
        return false;
    }
    return true;
}

void Config::load(const yaml &root)
{
    if (!check_config_root(root))
        return;

    load_settings(root);

    ProjectPath root_project;
    YAML_EXTRACT(root_project, String);

    auto prjs = root["projects"];
    if (prjs.IsDefined() && !prjs.IsMap())
        throw std::runtime_error("'projects' should be a map");

    auto add_project = [this, &root_project](auto &root, auto &&name)
    {
        Project project(root_project);
        project.defaults_allowed = defaults_allowed;
        project.allow_relative_project_names = allow_relative_project_names;
        project.allow_local_dependencies = allow_local_dependencies;
        project.is_local = is_local;
        project.load(root);
        if (project.name.empty())
            project.name = name;
        project.setRelativePath(name);
        projects.emplace(project.pkg.ppath.toString(), project);
    };

    projects.clear();
    if (prjs.IsDefined())
    {
        for (auto prj : prjs)
            add_project(prj.second, prj.first.template as<String>());
    }
    else
    {
        add_project(root, "");
    }
}

void Config::save(const path &dir)
{
    dump_yaml_config(dir / CPPAN_FILENAME, save());
}

yaml Config::save()
{
    yaml root;
    int i = 0;
    for (auto &p : projects)
    {
        auto n = p.first;
        if (n.empty())
            n = p.second.name;
        if (n.empty())
            n = "name" + std::to_string(i++);
        root["projects"][n] = p.second.save();
    }
    return root;
}

void Config::clear_vars_cache() const
{
    for (auto &f : boost::make_iterator_range(fs::recursive_directory_iterator(directories.storage_dir_cfg), {}))
    {
        if (!fs::is_regular_file(f))
            continue;
        remove_file(f);
    }
}

Project &Config::getProject1(const ProjectPath &ppath)
{
    if (projects.empty())
        throw std::runtime_error("Projects are empty");
    if (projects.size() == 1)
        return projects.begin()->second;
    auto i = projects.find(ppath.toString());
    if (i == projects.end())
        throw std::runtime_error("No such project '" + ppath.toString() + "' in config");
    return i->second;
}

Project &Config::getProject(const ProjectPath &ppath)
{
    return getProject1(ppath);
}

const Project &Config::getProject(const ProjectPath &ppath) const
{
    return (const Project &)((Config *)this)->getProject1(ppath);
}

Project &Config::getDefaultProject(const ProjectPath &ppath)
{
    if (ppath.empty() && projects.size() > 1)
        return projects.begin()->second;
    return getProject(ppath);
}

const Project &Config::getDefaultProject(const ProjectPath &ppath) const
{
    if (ppath.empty() && projects.size() > 1)
        return projects.begin()->second;
    return getProject(ppath);
}

void Config::process(const path &p) const
{
    rd.process(p, (Config&)*this);
}

void Config::post_download() const
{
    if (!created)
        return;

    auto &p = getDefaultProject();
    p.prepareExports();
    p.patchSources();

    // remove from table
    AccessTable at;
    at.remove(pkg.getDirSrc());
    at.remove(pkg.getDirObj());

    auto printer = Printer::create(Settings::get_local_settings().printerType);
    printer->d = pkg;
    printer->prepare_rebuild();
}

Packages Config::getFileDependencies() const
{
    Packages dependencies;
    for (auto &p : projects)
    {
        for (auto &d : p.second.dependencies)
        {
            // skip ill-formed deps
            if (d.second.ppath.is_relative())
                continue;
            dependencies.insert({ d.second.ppath.toString(), d.second });
        }
    }
    return dependencies;
}

void Config::setPackage(const Package &p)
{
    pkg = p;
    for (auto &project : projects)
    {
        // modify p
        // p.ppath = project.name;
        project.second.pkg = p;
    }
}

std::vector<Config> Config::split() const
{
    std::vector<Config> configs;
    for (auto &p : projects)
    {
        Config c = *this;
        c.projects.clear();
        c.projects.insert(p);
        configs.push_back(c);
    }
    return configs;
}
