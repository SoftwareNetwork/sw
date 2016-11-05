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

#include "common.h"
#include "filesystem.h"
#include "http.h"
#include "version.h"
#include "yaml.h"

#include "printers/printer.h"

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

    // sys/user config settings
    String host{ "https://cppan.org/" };
    ProxySettings proxy;
    ConfigType storage_dir_type{ ConfigType::User };
    path storage_dir;
    ConfigType build_dir_type{ ConfigType::System };
    path build_dir;
    path cppan_dir = ".cppan";
    // printer
    PrinterType printerType{ PrinterType::CMake };
    // do not check for new cppan version
    bool disable_update_checks = false;

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
    String generator;
    String toolset;
    String type{ "executable" };
    String library_type;
    String executable_type;

    std::map<String, String> env;
    std::vector<String> cmake_options;

    bool use_shared_libs = false;
    // do not create links to projects (.sln, CMakeLists.txt)
    bool silent = true;

    bool use_cache = true;
    bool show_ide_projects = false;
    // auto re-run cppan when spec file is changed
    bool add_run_cppan_target = false;

    // own data
    // maybe mutable?
    bool is_dir = false;
    bool rebuild = false;
    bool allow_links = true;
    String filename;
    String filename_without_ext;
    path source_directory;
    path binary_directory;
    String source_directory_hash;
    String config;

public:
    Settings();

    void load(const path &p, const ConfigType type);
    void load(const yaml &root, const ConfigType type);

    bool is_custom_build_dir() const;

    void load(const yaml &root);
    void set_build_dirs(const path &fn);
    void append_build_dirs(const path &p);
    String get_hash() const;
    String get_fs_generator() const;

    int generate(Config *c) const;
    int build(Config *c) const;
    int build_package(Config *c);

    bool checkForUpdates() const;

private:
    void load_main(const yaml &root, const ConfigType type);
    void load_build(const yaml &root);
};

String get_config(const Settings &settings);
String test_run(const Settings &settings);
