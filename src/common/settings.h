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
#include "filesystem.h"
#include "http.h"
#include "remote.h"
#include "version.h"
#include "yaml.h"

#include "printers/printer.h"

#include <primitives/executor.h>

#include <map>

void cleanConfig(const String &config);
void cleanConfigs(const Strings &configs);

struct BuildSettings
{
    bool allow_links = true;
    bool disable_checks = false;
    bool test_run = false;
    String filename;
    String filename_without_ext;
    path source_directory;
    path binary_directory;
    String source_directory_hash;
    String config;

    void set_build_dirs(const String &name);
    void append_build_dirs(const path &p);
};

struct Settings
{
    enum CMakeConfigurationType
    {
        Debug,
        MinSizeRel,
        Release,
        RelWithDebInfo,

        Max
    };

    // connection
    Remotes remotes{ get_default_remotes() };
    ProxySettings proxy;

    // sys/user config settings
    SettingsType storage_dir_type{ SettingsType::User };
    path storage_dir;
    SettingsType build_dir_type{ SettingsType::Local };
    path build_dir;
    path cppan_dir = ".cppan";
    path output_dir = "bin";
    // printer
    PrinterType printerType{ PrinterType::CMake };
    // do not check for new cppan version
    bool disable_update_checks = false;
    int max_download_threads = get_max_threads(8);
    bool debug_generated_cmake_configs = false;

    // build settings
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
    String default_configuration{ "Release" }; // for global settings
    String generator;
    String system_version;
    String toolset;

    std::map<String, String> env;
    std::vector<String> cmake_options;

    bool use_shared_libs = true;

    // do not create links to projects (.sln, CMakeLists.txt)
    bool silent = false;

    // number of parallel jobs for variable checks
    int var_check_jobs = 0;

    // level of warnings on dependencies
    int build_warning_level = 0;

    // following settings can be overriden in current build config
    bool use_cache = true;
    bool show_ide_projects = false;
    // auto re-run cppan when spec file is changed
    bool add_run_cppan_target = false;
    bool cmake_verbose = false;
    bool build_system_verbose = true;
    bool force_server_query = false;
    bool verify_all = false;
    bool copy_all_libraries_to_output = false;
    bool copy_import_libs = false;
    bool full_path_executables = false;
    bool rc_enabled = true;
    bool short_local_names = false;

    String install_prefix;

    // for build command
    Strings additional_build_args;

    // to differentiate different cmake invocations
    String meta_target_suffix;

    // some project-like variables
    // later this could be replaced with local_settings' Config c;
    Packages dependencies;

    // not for loading from config
    bool generate_only = false;
    bool load_project = true;
    bool can_update_packages_db = true;

public:
    Settings();

    void load(const path &p, const SettingsType type);
    void load(const yaml &root, const SettingsType type);
    void save(const path &p) const;

    bool is_custom_build_dir() const;
    String get_hash() const;
    bool checkForUpdates() const;

private:
    void load_main(const yaml &root, const SettingsType type);
    void load_build(const yaml &root);

public:
    static Settings &get(SettingsType type);
    static Settings &get_system_settings();
    static Settings &get_user_settings();
    static Settings &get_local_settings();
    static void clear_local_settings();
};
