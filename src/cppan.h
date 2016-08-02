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

#pragma once

#include <map>
#include <set>

#include <yaml-cpp/yaml.h>

#include "common.h"
#include "dependency.h"
#include "filesystem.h"
#include "project_path.h"
#include "property_tree.h"

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

using Definitions = std::multimap<String, String>;
using Files = std::set<path>;
using Sources = std::set<String>;
using StringSet = std::set<String>;
using Symbols = std::map<String, StringSet>;

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
    StringSet global_definitions;
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
    Files exclude_from_build;
    BuildSystemConfigInsertions bs_insertions;
    std::map<String, Options> options;
    StringSet aliases;
    bool load_from_bazel = false;

    // no files to compile
    bool header_only = false;

    // no files (cmake only etc.)
    bool empty = false;

    // library type
    bool shared_only = false;
    bool static_only = false;

    // files to include into archive
    Files files;

    // this file
    String cppan_filename;

    // root_directory where all files is stored
    path root_directory;

    void findSources(path p);
    bool writeArchive(const String &filename) const;
};

using Projects = std::map<String, Project>;

PackagesDirType packages_dir_type_from_string(const String &s);

struct Config
{
    String host{ "https://cppan.org/" };
    ProxySettings proxy;
    PackagesDirType packages_dir_type{ PackagesDirType::User };
    path storage_dir;

    Version version;
    Source source;

    ProjectPath root_project;

    StringSet check_functions;
    StringSet check_includes;
    StringSet check_types;
    Symbols check_symbols;
    StringSet check_libraries;
    BuildSystemConfigInsertions bs_insertions;
    std::map<String, Options> options;
    std::map<String, Options> global_options;

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

	//void parse(YAML::Node &r, Config &c, const path &p);
};
