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

#include "checks.h"
#include "common.h"
#include "context.h"
#include "filesystem.h"
#include "http.h"
#include "package.h"
#include "project.h"
#include "project_path.h"
#include "property_tree.h"
#include "version.h"
#include "yaml.h"

#include "printers/printer.h"

struct Config;
struct LocalSettings;

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
    bool silent = true;

    // own data
    bool is_dir = false;
    bool rebuild = false;
    bool prepare = true;
    String filename;
    String filename_without_ext;
    path source_directory;
    path binary_directory;
    String source_directory_hash;
    String config;
    bool allow_links = true;

    void load(const yaml &root);
    void prepare_build(Config *c, const path &fn, const String &cppan, bool force = false);
    void set_build_dirs(const path &fn);
    void append_build_dirs(const path &p);
    String get_hash() const;
    String get_fs_generator() const;
};

struct LocalSettings
{
    // sys/user config settings
    String host{ "https://cppan.org/" };
    ProxySettings proxy;
    ConfigType storage_dir_type{ ConfigType::User };
    path storage_dir;
    ConfigType build_dir_type{ ConfigType::System };
    path build_dir;
    path cppan_dir = ".cppan";
    bool use_cache = true;
    bool show_ide_projects = false;
    bool add_run_cppan_target = false;
    bool disable_update_checks = false;
    BuildSettings build_settings;

    LocalSettings();

    void load(const path &p, const ConfigType type);
    void load(const yaml &root, const ConfigType type);
    String get_hash() const;

    bool is_custom_build_dir() const;

private:
    void load_main(const yaml &root);
};

struct Config
{
    // own data
    ConfigType type{ ConfigType::None };
    PrinterType printerType{PrinterType::CMake};
    bool defaults_allowed = true;

    //
    LocalSettings local_settings;

    // source (git, remote etc.)
    Version version;
    Source source;

    // projects settings
    ProjectPath root_project;
    Checks checks;
    BuildSystemConfigInsertions bs_insertions;

    // own data
    std::map<String, Options> options;
    std::map<String, Options> global_options;

    Config();
    Config(ConfigType type);
    Config(const path &p);

    void load(yaml root, const path &p = CPPAN_FILENAME);
    void load(const path &p);
    void save(const path &p) const;

    static Config get_system_config();
    static Config get_user_config();
    void load_current_config();

    void prepare_build(path fn, const String &cppan);
    int generate() const;
    int build() const;

    void process(const path &p = path());
    void post_download() const;

    void clear_vars_cache(path p) const;

    Projects &getProjects() { return projects; }
    Project &getDefaultProject();
    const Project &getDefaultProject() const;
    Project &getProject(const String &p) const;

    void setPackage(const Package &pkg);

    Packages getFileDependencies() const; // from file

    void checkForUpdates() const;

private:
    Projects projects;
    path dir;

    void addDefaultProject();

public:
    struct InternalOptions
    {
        Package current_package;
        std::set<Package> invocations;
    } internal_options;

    bool is_printed = false;
    bool disable_run_cppan_target = false;

    bool is_dependency = false;

    // we create this project for the first time (downloaded, locally created etc.)
    bool created = false;

    // current package
    Package pkg;
};
