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

#include "common.h"
#include "dependency.h"
#include "project_path.h"
#include "source.h"
#include "yaml.h"

#include <unordered_map>

#define BAZEL_BUILD_FILE "BUILD"
#define DEPENDENCIES_NODE "dependencies"
#define INCLUDE_DIRECTORIES_ONLY "include_directories_only"

using Definitions = std::multimap<String, String>;
using CompileOptions = std::multimap<String, String>;
using LinkOptions = std::multimap<String, String>;
using LinkLibraries = std::multimap<String, String>;

using SystemDefinitions = std::map<String, Definitions>;
using SystemCompileOptions = std::map<String, CompileOptions>;
using SystemLinkOptions = std::map<String, LinkOptions>;
using SystemLinkLibraries = std::map<String, LinkLibraries>;

using Sources = std::set<String>;
using StringMap = std::map<String, String>;
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
    CompileOptions compile_options;
    LinkOptions link_options;
    LinkLibraries link_libraries;

    SystemDefinitions system_definitions;
    SystemCompileOptions system_compile_options;
    SystemLinkOptions system_link_options;
    SystemLinkLibraries system_link_libraries;

    BuildSystemConfigInsertions bs_insertions;

    StringSet include_directories;
    StringSet link_directories;
    StringSet global_definitions;
};

using ReplaceInFiles = std::unordered_map<String, String>;

struct Patch
{
    ReplaceInFiles replace_in_files;

    void load(const yaml &root);
};

struct Project
{
    // public data

    // source (git, remote etc.)
    Version version;
    Source source;

    ProjectPath ppath;
    String license;
    IncludeDirectories include_directories;
    Sources sources;
    Sources build_files;
    Sources exclude_from_package;
    Packages dependencies;
    Files exclude_from_build;
    BuildSystemConfigInsertions bs_insertions;
    std::map<String, Options> options;
    Patch patch;
    StringSet aliases;
    bool import_from_bazel = false;
    bool copy_to_output_dir = true;
    bool prefer_binaries = false;

    // no files to compile
    bool header_only = false;

    // no files (cmake only etc.)
    bool empty = false;

    // library type
    bool shared_only = false;
    bool static_only = false;

    // c/c++ standard
    int c_standard{ 0 };
    int cxx_standard{ 0 };

    // files to include into archive
    // also is used for enumerating sources (mutable for this)
    mutable Files files;

    // root_directory where all files are stored
    path root_directory;

    // directory where all files are stored after unpack
    path unpack_directory;

    // current package: ppath+version+flags
    Package pkg;

    // allow default values if some parts are missing
    bool defaults_allowed = true;

public:
    Project(const ProjectPath &root_project);

    void findSources(path p);
    bool writeArchive(const String &filename) const;
    void prepareExports() const;
    void patchSources() const;

    void setRelativePath(const ProjectPath &root_project, const String &name);

    void load(const yaml &root);
    void save_dependencies(yaml &root) const;

private:
    ProjectPath root_project;

    const Files &getSources() const;
};

using Projects = std::map<String, Project>;

void load_source_and_version(const yaml &root, Source &source, Version &version);
