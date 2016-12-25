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

#include "cppan_string.h"
#include "dependency.h"
#include "optional.h"
#include "project_path.h"
#include "source.h"
#include "yaml.h"

#include <unordered_map>

#define DEPENDENCIES_NODE "dependencies"
#define INCLUDE_DIRECTORIES_ONLY "include_directories_only"

using Sources = std::set<String>;
using StringMap = std::map<String, String>;
using StringSet = std::set<String>;

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

    void load(const yaml &n);
    void save(yaml &n) const;
};

struct Options
{
public:
    using ValueContainer = std::set<std::pair<String, String>>;

    using Definitions = ValueContainer;
    using IncludeDirectories = ValueContainer;
    using CompileOptions = ValueContainer;
    using LinkOptions = ValueContainer;
    using LinkLibraries = ValueContainer;

    using SystemDefinitions = std::map<String, Definitions>;
    using SystemIncludeDirectories = std::map<String, IncludeDirectories>;
    using SystemCompileOptions = std::map<String, CompileOptions>;
    using SystemLinkOptions = std::map<String, LinkOptions>;
    using SystemLinkLibraries = std::map<String, LinkLibraries>;

public:
    Definitions definitions;
    IncludeDirectories include_directories;
    CompileOptions compile_options;
    LinkOptions link_options;
    LinkLibraries link_libraries;

    SystemDefinitions system_definitions;
    SystemIncludeDirectories system_include_directories;
    SystemCompileOptions system_compile_options;
    SystemLinkOptions system_link_options;
    SystemLinkLibraries system_link_libraries;

    StringSet link_directories;

    BuildSystemConfigInsertions bs_insertions;
};

using OptionsMap = std::map<String, Options>;

OptionsMap loadOptionsMap(const yaml &root);
void saveOptionsMap(yaml &root, const OptionsMap &m);

using ReplaceInFiles = std::unordered_map<String, String>;

struct Patch
{
    ReplaceInFiles replace_in_files;

    void load(const yaml &root);
    void save(yaml &root) const;
};

struct Project
{
    // public data
public:
    // source (git, remote etc.)
    Version version;
    Source source;

    ProjectPath ppath;
    String license;
    IncludeDirectories include_directories;
    // files to compile only
    // when not empty it will be main source to take files from
    Sources sources;
    Sources build_files;
    Sources exclude_from_package;
    Sources exclude_from_build;
    Packages dependencies;
    BuildSystemConfigInsertions bs_insertions;
    OptionsMap options;
    Patch patch;
    StringSet aliases;

    // no files (cmake only etc.)
    bool empty = false;

    // do not check mime types
    // project may contain different files
    bool custom = false;

    // library type
    bool shared_only = false;
    bool static_only = false;

    // c/c++ standard
    int c_standard{ 0 };
    int cxx_standard{ 0 };

    bool import_from_bazel = false;
    bool prefer_binaries = false;
    bool export_all_symbols = false;
    bool build_dependencies_with_same_config = false;

    String api_name;

    // files to include into archive
    // also is used for enumerating sources (mutable for this)
    mutable Files files;

    // root_directory where all files are stored
    path root_directory;

    // directory where all files are stored after unpack
    path unpack_directory;

    // current package: ppath+version+flags
    Package pkg;

    // optional
    String name;
    ProjectType type{ ProjectType::Executable };
    LibraryType library_type{ LibraryType::Static };
    ExecutableType executable_type{ ExecutableType::Default };

    // allow default values if some parts are missing
    bool defaults_allowed = true;

    // allow relative project paths
    bool allow_local_dependencies = false;

    // allow relative project paths
    bool allow_relative_project_names = false;

    // private data
private:
    // no files to compile
    optional<bool> header_only;

public:
    Project();
    Project(const ProjectPath &root_project);

    void applyFlags(ProjectFlags &flags) const;
    void addDependency(const Package &p);

    void findSources(path p);
    bool writeArchive(const path &fn) const;
    void prepareExports() const;
    void patchSources() const;

    void setRelativePath(const String &name);

    void load(const yaml &root);
    yaml save() const;
    void save_dependencies(yaml &root) const;

    // own data, not from config
public:
    // flag shows that files were loaded from 'files' node
    bool files_loaded = false;

    // cloned data from input file for future save() calls
    std::shared_ptr<Project> original_project;

private:
    ProjectPath root_project;

    const Files &getSources() const;
    ProjectPath relative_name_to_absolute(const String &name);
    optional<ProjectPath> load_local_dependency(const String &name);
    void findRootDirectory(const path &p, int depth = 0);
};

using Projects = std::map<String, Project>;

void load_source_and_version(const yaml &root, Source &source, Version &version);
