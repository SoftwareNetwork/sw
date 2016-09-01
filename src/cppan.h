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

#include "common.h"
#include "context.h"
#include "dependency.h"
#include "filesystem.h"
#include "project.h"
#include "project_path.h"
#include "property_tree.h"
#include "yaml.h"

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

    std::map<String, String> env;
    std::vector<String> cmake_options;

    bool use_shared_libs = false;
    bool silent =
#ifdef _WIN32
        false
#else
        true
#endif
        ;
    bool rebuild = false;

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

    void clean_cmake_cache(path p) const;
    void clean_vars_cache(path p) const;
    void clean_cmake_exports(path p) const;

    void prepare_build(path fn, const String &cppan);
    int generate() const;
    int build() const;

    Projects &getProjects() { return projects; }
    Project &getDefaultProject();
    const Project &getProject(const String &p) const;

    Dependencies getDirectDependencies() const; // from server
    Dependencies getIndirectDependencies() const; // from server
    Dependencies getDependencies() const; // from file

    path get_storage_dir(PackagesDirType type) const;
    path get_storage_dir_bin() const;
    path get_storage_dir_cfg() const;
    path get_storage_dir_etc() const;
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
    class AccessTable *access_table = nullptr;

    void load_common(const path &p);
    void load_common(const yaml &root);

    void print_meta_config_file() const;
    void print_include_guards_file() const;
    void print_helper_file() const;
    void print_package_config_file(const path &config_file, const DownloadDependency &d, const Config &parent) const;
    void print_package_actions_file(const path &config_dir, const DownloadDependency &d) const;
    void print_package_include_file(const path &config_dir, const DownloadDependency &d, const String &ig) const;
    void print_object_config_file(const path &config_file, const DownloadDependency &d, const Config &parent) const;
    void print_object_include_config_file(const path &config_file, const DownloadDependency &d) const;
    void print_object_export_file(const path &config_dir, const DownloadDependency &d) const;
    void print_object_build_file(const path &config_dir, const DownloadDependency &d) const;
    void print_bs_insertion(Context &ctx, const Project &p, const String &name, const String BuildSystemConfigInsertions::*i) const;

    void download_and_unpack(const String &data_url) const;
    void print_configs();
    void extractDependencies(const ptree &dependency_tree);

public:
    struct InternalOptions
    {
        DownloadDependency current_package;
        std::set<Dependency> invocations;
    } internal_options;
};
