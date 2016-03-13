/*
 * C++ Archive Network Client
 * Copyright (C) 2016 Egor Pugin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <map>
#include <set>

#include <yaml-cpp/yaml.h>

#include <common.h>
#include <dependency.h>
#include <filesystem.h>
#include <project_path.h>
#include <property_tree.h>

path get_home_directory();
path get_root_directory();
path get_config_filename();

struct PackageInfo
{
    String target_name;
    String variable_name;
    std::shared_ptr<Dependency> dependency;

    PackageInfo() {}
    PackageInfo(const Dependency &d);
};

using Definitions = std::map<String, String>;
using Files = std::set<path>;
using Sources = std::set<String>;
using StringSet = std::set<String>;

enum class BuildType
{
    CMake,
    Makefile,
    VsProject,
    Qmake,
    Ninja
};

struct IncludeDirectories
{
    Files public_;
    Files private_;

    bool empty() const
    {
        return public_.empty() && private_.empty();
    }
};

struct BuildSystemConfigInsertions
{
    String pre_sources;
    String post_sources;
    String post_target;
    String post_alias;

    void get_config_insertions(const YAML::Node &n);
};

struct Options
{
    Definitions definitions;
    BuildSystemConfigInsertions bs_insertions;
    StringSet include_directories;
    StringSet link_directories;
    StringSet link_libraries;
};

struct Project
{
    ProjectPath package;
    String license;
    IncludeDirectories include_directories;
    Sources sources;
    Sources build_files;
    BuildType buildType;
    Dependencies dependencies;
    Dependencies dependencies_private;
    Files exclude_from_build;
    BuildSystemConfigInsertions bs_insertions;
    std::map<String, Options> options;

    //
    bool header_only = false;
    bool empty = false;

    // files to include into archive
    Files files;

    // this file
    String cppan_filename;

    // root_directory where all files is stored
    path root_directory;

    void findSources(path p);
    bool writeArchive(const String &filename) const;
};

using Projects = std::vector<Project>;

PackagesDirType packages_dir_type_from_string(const String &s);

struct Config
{
    String host{ "https://cppan.org/" };
    PackagesDirType packages_dir_type{ PackagesDirType::User };
    path storage_dir;
    ProjectPath root_project;

    StringSet check_functions;
    StringSet check_includes;
    StringSet check_types;
    StringSet check_symbols;
    StringSet check_libraries;
    BuildSystemConfigInsertions bs_insertions;
    std::map<String, Options> options;

    Config();
    Config(const path &p);

    void load(const path &p);
    void save(const path &p) const;

    static Config load_system_config();
    static Config load_user_config();
    void load_current_config();

    void download_dependencies();
    void create_build_files() const;

    Projects &getProjects() { return projects; }

private:
    ptree dependency_tree;
    mutable std::map<String, PackageInfo> packages;
    Dependencies indirect_dependencies;
    Projects projects;

    void load_common(const path &p);
    void load_common(const YAML::Node &root);
    Project load_project(const YAML::Node &root);
    ProjectPath relative_name_to_absolute(const String &name);
    path get_packages_dir(PackagesDirType type);

    void download_and_unpack(const ptree &deps, Dependency &parent);
    void create_build_files1(const Dependencies &deps) const;
    void print_meta_config_file() const;
    void print_helper_file() const;
    PackageInfo print_package_config_file(std::ofstream &o, const Dependency &d, Config &parent) const;
};
