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
#include "filesystem.h"
#include "package.h"
#include "project.h"
#include "project_path.h"
#include "property_tree.h"
#include "yaml.h"

#include "printers/printer.h"

struct Directories
{
    path storage_dir;
    path storage_dir_bin;
    path storage_dir_cfg;
    path storage_dir_etc;
    path storage_dir_lib;
    path storage_dir_obj;
    path storage_dir_src;
    path storage_dir_usr;
    path build_dir;

    PackagesDirType storage_dir_type;
    PackagesDirType build_dir_type;

    bool empty() const { return storage_dir.empty(); }

    void set_storage_dir(const path &p);
    void set_build_dir(const path &p);
};

extern Directories directories;

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
    String filename;
    String filename_without_ext;
    path source_directory;
    path binary_directory;
    String config;
    Config *c;
    LocalSettings *ls;

    BuildSettings(Config *c, LocalSettings *ls);

    void load(const yaml &root);
    void prepare_build(const path &fn, const String &cppan);
    void set_build_dirs(const path &fn);
    void append_build_dirs(const path &p);
    void set_config(Config *config);
    String get_hash() const;
};

struct LocalSettings
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

    // own data
    Config *c;

    LocalSettings(Config *c);

    void load(const path &p);
    void load(const yaml &root);
    void set_config(Config *config);
    String get_hash() const;

private:
    void load_main(const yaml &root);
};

struct Config
{
    PrinterType printerType{PrinterType::CMake};

    //
    LocalSettings local_settings;

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

    // own data
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

    void prepare_build(path fn, const String &cppan);
    int generate() const;
    int build() const;

    void process(const path &p = path());
    void post_download() const;

    void clear_vars_cache(path p) const;

    Projects &getProjects() { return projects; }
    Project &getDefaultProject() const;
    Project &getProject(const String &p) const;

    Packages getFileDependencies() const; // from file

private:
    Projects projects;
    path dir;

    bool need_probe() const;

public:
    struct InternalOptions
    {
        Package current_package;
        std::set<Package> invocations;
    } internal_options;

    bool is_printed = false;
    bool disable_run_cppan_target = false;

    bool is_dependency = false;
    bool downloaded = false;
    Package pkg; // current package
};
