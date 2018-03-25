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

#include "checks.h"
#include "cppan_string.h"
#include "dependency.h"
#include "project_path.h"
#include "source.h"
#include "yaml.h"

#include <primitives/stdcompat/optional.h>

#include <unordered_map>

#define DEPENDENCIES_NODE "dependencies"
#define INCLUDE_DIRECTORIES_ONLY "include_directories_only"

using Sources = std::set<String>;






enum class CompilerType
{
    Clang,
    Gnu,
    Msvc,
    Intel,
    // more
};

enum class LinkType
{
    Any,
    Static,
    Shared,
};

enum class Visibility
{
    Public,
    Private,
    Interface,
    Other,
};

enum class Os
{
    Windows,
    Linux,
    Macos,
};

enum class Arch
{
    x86,
    x64,
    arm,
    // more from clang
};
// subarch

struct Compiler
{
    CompilerType type;
    Version version;
};

struct Option
{
    enum Type
    {
        Definition,
        IncludeDirectory,
        CompileOption,
        LinkOption,
        LinkLibrary,
    };

    Type type;
    String name;
    Visibility visibility;
    LinkType linkType;
    CompilerType compilerType;
    String condition;
};






struct IncludeDirectories
{
    Files public_;
    Files private_;
    Files interface_;

    bool empty() const
    {
        return public_.empty() && private_.empty() && interface_.empty();
    }
};

struct BuildSystemConfigInsertions
{
#define BSI(x) String x;
#include "bsi.inl"
#undef BSI

    void load(const yaml &n);
    void save(yaml &n) const;

    static void merge(yaml &dst, const yaml &src);
    static void merge_and_remove(yaml &dst, yaml &src);
    static void remove(yaml &src);
    static Strings getStrings();
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

using ReplaceInFiles = std::vector<std::pair<String, String>>;

struct Patch
{
    ReplaceInFiles replace;
    ReplaceInFiles regex_replace;

    void load(const yaml &root);
    void save(yaml &root) const;
    void patchSources(const Files &files) const;
};

struct Project
{
    // public data
public:
    Source source;
    Package pkg;
    String license;
    IncludeDirectories include_directories;

    // files to compile only
    // when not empty it will be main source to take files from
    Sources sources;

    Sources build_files;
    Sources exclude_from_package;
    Sources exclude_from_build;

    Sources public_headers;
    Sources include_hints;

    Packages dependencies;
    BuildSystemConfigInsertions bs_insertions;
    OptionsMap options;
    Patch patch;
    StringSet aliases;
    Checks checks;
    StringSet checks_prefixes;

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
    bool c_extensions = false;
    int cxx_standard{ 0 };
    bool cxx_extensions = false;

    bool import_from_bazel = false;
    String bazel_target_function;
    String bazel_target_name;

    bool prefer_binaries = false;
    bool export_all_symbols = false;
    bool export_if_static = false;
    bool build_dependencies_with_same_config = false;
    bool rc_enabled = true;
    bool disabled = false;

    StringSet api_name;
    String output_name; // file name
    String condition; // when this target is active

    // files to include into archive
    // also is used for enumerating sources (mutable for this)
    mutable Files files;

    // root_directory where all files are stored
    path root_directory;

    // directory where all files are stored after unpack
    path unpack_directory;

    // directory where binary files are copied, no need to be a path
    String output_directory;

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

    // package processed is not in storage on client system
    bool is_local = true;

    // private data
private:
    // no files to compile
    optional<bool> header_only;

public:
    Project();
    Project(const ProjectPath &root_project);

    void applyFlags(ProjectFlags &flags) const;
    void addDependency(const Package &p);

    void findSources(path p = path());
    bool writeArchive(const path &fn) const;
    void prepareExports() const;
    void patchSources() const;

    void setRelativePath(const String &name);

    void load(const yaml &root);
    yaml save() const;
    void save_dependencies(yaml &root) const;

    String print_cpp();
    String print_cpp2();

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
};

using Projects = std::map<String, Project>;

void load_source_and_version(const yaml &root, Source &source, Version &version);

inline const auto bazel_filenames = { "BUILD", "BUILD.bazel" };
