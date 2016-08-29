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

#define DEPENDENCIES_NODE "dependencies"

using yaml = YAML::Node;

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
using Sources = std::set<String>;
using StringSet = std::set<String>;
using Symbols = std::map<String, StringSet>;

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

    void get_config_insertions(const yaml &n);
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
    Source source;
    ProjectPath package;
    String license;
    IncludeDirectories include_directories;
    Sources sources;
    Sources build_files;
    Dependencies dependencies;
    Files exclude_from_build;
    BuildSystemConfigInsertions bs_insertions;
    std::map<String, Options> options;
    StringSet aliases;
    bool import_from_bazel = false;

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

    void save_dependencies(yaml &root) const;
};

using Projects = std::map<String, Project>;

PackagesDirType packages_dir_type_from_string(const String &s, const String &key);

struct BuildSettings
{
    enum CMakeConfigurationType
    {
        Debug,
        MinSizeRel,
        Release,
        RelWithDebInfo,

        Max
    };

    // from settings
    String c_compiler;
    String cxx_compiler;
    String compiler;
    String c_compiler_flags;
    String c_compiler_flags_conf[CMakeConfigurationType::Max];
    String cxx_compiler_flags;
    String cxx_compiler_flags_conf[CMakeConfigurationType::Max];
    String compiler_flags;
    String compiler_flags_conf[CMakeConfigurationType::Max];
    String link_flags;
    String link_flags_conf[CMakeConfigurationType::Max];
    String link_libraries;
    String configuration{ "Release" };
    String generator;
    String toolset;
    String type{ "executable" };
    String library_type;
    String executable_type;
    bool use_shared_libs = false;
    bool silent =
#ifdef _WIN32
        false
#else
        true
#endif
        ;

    // own data
    bool is_dir = false;
    String filename;
    String filename_without_ext;
    path source_directory;
    path binary_directory;

    void load(const yaml &root);
};

struct Config
{
    // sys/user config settings
    String host{ "https://cppan.org/" };
    ProxySettings proxy;
    PackagesDirType storage_dir_type{ PackagesDirType::User };
    path storage_dir;
    PackagesDirType build_dir_type{ PackagesDirType::System };
    path build_dir;
    bool local_build = false;
    bool show_ide_projects = false;
    bool add_run_cppan_target = false;
    BuildSettings build_settings;

    // source (git, remote etc.)
    Version version;
    Source source;

    // projects settings
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

    void load(const yaml &root, const path &p = CPPAN_FILENAME);
    void load(const path &p);
    void save(const path &p) const;

    static Config load_system_config();
    static Config load_user_config();
    void load_current_config();

    void process();
    void download_dependencies();
    void create_build_files() const;

    void prepare_build(path fn, const String &cppan);
    int generate() const;
    int build() const;

    Projects &getProjects() { return projects; }
    Project &getDefaultProject();
    const Project *getProject(const String &p) const;

    static Source load_source(const yaml &root);
    static void save_source(yaml &root, const Source &source);

    Dependencies getDirectDependencies() const; // from server
    Dependencies getIndirectDependencies() const; // from server
    Dependencies getDependencies() const; // from file

    path get_storage_dir(PackagesDirType type) const;
    path get_storage_dir_bin() const;
    path get_storage_dir_lib() const;
    path get_storage_dir_obj() const;
    path get_storage_dir_src() const;
    path get_storage_dir_user_obj() const;

    path get_build_dir(PackagesDirType type) const;

private:
    bool printed = false;
    bool disable_run_cppan_target = false;
    ptree dependency_tree;
    DownloadDependencies dependencies;
    Projects projects;
    mutable std::set<String> include_guards;
    path dir;

    void load_common(const path &p);
    void load_common(const yaml &root);
    Project load_project(const yaml &root, const String &name);
    ProjectPath relative_name_to_absolute(const String &name);

    void print_meta_config_file() const;
    void print_helper_file() const;
    void print_package_config_file(const path &config_file, const DownloadDependency &d, Config &parent) const;
    void print_package_include_file(const path &config_file, const DownloadDependency &d, Config &parent) const;
    void print_object_config_file(const path &config_file, const DownloadDependency &d, const Config &parent) const;
    void print_object_include_config_file(const path &config_file, const DownloadDependency &d) const;

    void download_and_unpack(const String &data_url) const;
    void print_configs();
    void extractDependencies(const ptree &dependency_tree);
    void process_response_file();

public:
    struct InternalOptions
    {
        DownloadDependency current_package;
        std::set<Dependency> invocations;
    } internal_options;
};
