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

#include "config.h"

#include "access_table.h"
#include "context.h"
#include "database.h"
#include "directories.h"
#include "lock.h"
#include "hash.h"
#include "hasher.h"
#include "resolver.h"
#include "yaml.h"

#include <boost/algorithm/string.hpp>

#include <fstream>
#include <iostream>

#include "logger.h"
DECLARE_STATIC_LOGGER(logger, "config");

Config::Config()
{
    addDefaultProject();
}

Config::Config(ConfigType type)
    : type(type)
{
    switch (type)
    {
    case ConfigType::System:
    {
        auto fn = CONFIG_ROOT "default";
        if (!fs::exists(fn))
            break;
        // do not move after the switch
        // it should not be executed there
        settings.load(fn, type);
    }
        break;
    case ConfigType::User:
    {
        auto fn = get_config_filename();
        if (!fs::exists(fn))
        {
            boost::system::error_code ec;
            fs::create_directories(fn.parent_path(), ec);
            if (ec)
                throw std::runtime_error(ec.message());
            Config c = get_system_config();
            c.save(fn);
        }
        settings.load(fn, type);
    }
    break;
    }
    addDefaultProject();
}

Config::Config(const path &p)
    : Config()
{
    reload(p);
}

void Config::reload(const path &p)
{
    if (fs::is_directory(p))
    {
        dir = p;
        ScopedCurrentPath cp(p);
        load_current_config();
    }
    else
    {
        dir = p.parent_path();
        load(p);
    }
}

Config &Config::get_system_config()
{
    static Config c(ConfigType::System);
    return c;
}

Config &Config::get_user_config()
{
    static Config c(ConfigType::User);
    return c;
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

void Config::load(const path &p)
{
    auto root = load_yaml_config(p);
    load(root);
}

void Config::load(yaml root)
{
    if (root.IsNull() || !root.IsMap())
    {
        addDefaultProject();
        LOG_DEBUG(logger, "Spec file should be a map");
        return;
    }

    auto ls = root["local_settings"];
    if (ls.IsDefined())
    {
        if (!ls.IsMap())
            throw std::runtime_error("'local_settings' should be a map");
        settings.load(root["local_settings"], type);
    }
    else
    {
        // read user/system settings first
        auto uc = get_user_config();
        settings = uc.settings;
    }

    EXTRACT(root_project, String);
    checks.load(root);

    auto prjs = root["projects"];
    if (prjs.IsDefined() && !prjs.IsMap())
        throw std::runtime_error("'projects' should be a map");

    auto add_project = [this](auto &root, auto &&name)
    {
        Project project(root_project);
        project.defaults_allowed = defaults_allowed;
        project.allow_relative_project_names = allow_relative_project_names;
        project.allow_local_dependencies = allow_local_dependencies;
        project.load(root);
        if (project.name.empty())
            project.name = name;
        project.setRelativePath(root_project, name);
        projects.emplace(project.ppath.toString(), project);
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

void Config::clear_vars_cache() const
{
    for (auto &f : boost::make_iterator_range(fs::recursive_directory_iterator(directories.storage_dir_cfg), {}))
    {
        if (!fs::is_regular_file(f))
            continue;
        remove_file(f);
    }
}

Project &Config::getProject(const String &pname) const
{
    const Project *p = nullptr;
    if (projects.size() == 1)
        p = &projects.begin()->second;
    else if (!projects.empty())
    {
        auto it = projects.find(pname);
        if (it != projects.end())
            p = &it->second;
    }
    if (!p)
        throw std::runtime_error("No such project '" + pname + "' in dependencies list");
    return (Project &)*p;
}

Project &Config::getDefaultProject()
{
    if (projects.empty())
        throw std::runtime_error("Projects are empty");
    return projects.begin()->second;
}

const Project &Config::getDefaultProject() const
{
    if (projects.empty())
        throw std::runtime_error("Projects are empty");
    return projects.begin()->second;
}

void Config::save(const path &p) const
{
    std::ofstream o(p.string());
    if (!o)
        throw std::runtime_error("Cannot open file: " + p.string());
    yaml root;
    root["remotes"][DEFAULT_REMOTE_NAME]["url"] = settings.remotes[0].url;
    root["storage_dir"] = settings.storage_dir.string();
    o << YAML::Dump(root);
}

void Config::process(const path &p)
{
    rd.process(p, *this);
}

void Config::post_download() const
{
    if (!created)
        return;

    auto &p = getDefaultProject();
    p.prepareExports();
    p.patchSources();

    // remove from table
    AccessTable at(directories.storage_dir_etc);
    at.remove(pkg.getDirSrc());
    at.remove(pkg.getDirObj());

    auto printer = Printer::create(settings.printerType);
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
