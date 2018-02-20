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

#pragma once

#include <map>
#include <set>

#include "cppan_string.h"
#include "filesystem.h"
#include "package.h"
#include "project.h"
#include "project_path.h"
#include "yaml.h"

#define CONFIG_ROOT "/etc/cppan/"

struct Config
{
    Config();
    Config(const path &p, bool local = true);

    void load(const yaml &root);
    void load(const path &p);
    void load(const String &s);
    void reload(const path &p);

    void load_current_config();
    void load_current_config_settings();

    void save(const path &dir);
    yaml save();

    void process(const path &p = path()) const;
    void post_download() const;

    void clear_vars_cache() const;

    auto &getProjects() { return projects; }
    auto &getProjects() const { return projects; }
    Project &getDefaultProject(const ProjectPath &ppath = ProjectPath());
    const Project &getDefaultProject(const ProjectPath &ppath = ProjectPath()) const;
    Project &getProject(const ProjectPath &ppath);
    const Project &getProject(const ProjectPath &ppath) const;

    void setPackage(const Package &pkg);

    // split current conf into several with only one project in it
    std::vector<Config> split() const;

    Packages getFileDependencies() const; // from file

private:
    Projects projects;
    path dir; // cwd

    void addDefaultProject();
    Project &getProject1(const ProjectPath &ppath);

    bool check_config_root(const yaml &root);
    void load_settings(const yaml &root, bool load_project = true);

public:
    bool defaults_allowed = true;
    bool allow_relative_project_names = false;
    bool allow_local_dependencies = false;
    bool is_local = true;

    // we create this project for the first time (downloaded, locally created etc.)
    bool created = false;

    // current package
    Package pkg;
};
