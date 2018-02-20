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

#include "cppan_string.h"
#include "dependency.h"

struct Config;
class ProjectPath;

class PackageStore
{
public:
    struct PackageConfig
    {
        Config *config;
        Packages dependencies;
    };
    using PackageConfigs = std::unordered_map<Package, PackageConfig>;

    using iterator = PackageConfigs::iterator;
    using const_iterator = PackageConfigs::const_iterator;

public:
    void resolve_dependencies(const Config &c);
    std::tuple<PackagesSet, Config, String>
    read_packages_from_file(path p, const String &config_name = String(), bool direct_dependency = false);
    bool has_local_package(const ProjectPath &ppath) const;
    path get_local_package_dir(const ProjectPath &ppath) const;
    void process(const path &p, Config &root);

    Config *add_config(std::unique_ptr<Config> &&config, bool created);
    Config *add_config(const Package &p, bool local = true);
    Config *add_local_config(const Config &c);

    bool rebuild_configs() const { return has_downloads() || deps_changed; }
    bool has_downloads() const { return downloads > 0; }

public:
    PackageConfig &operator[](const Package &p);
    const PackageConfig &operator[](const Package &p) const;

    iterator begin();
    iterator end();

    const_iterator begin() const;
    const_iterator end() const;

    iterator find(const PackageConfigs::key_type &k) { return packages.find(k); }
    const_iterator find(const PackageConfigs::key_type &k) const { return packages.find(k); }

    bool empty() const { return packages.empty(); }
    size_t size() const { return packages.size(); }

private:
    PackageConfigs packages;
    std::set<std::unique_ptr<Config>> config_store;

    std::unordered_map<Package, Package> resolved_packages;
    std::unordered_map<ProjectPath, path> local_packages;

    bool processing = false;
    int downloads = 0;
    bool deps_changed = false;

    void write_index() const;
    void check_deps_changed();

    friend class Resolver;
};

extern PackageStore rd;
