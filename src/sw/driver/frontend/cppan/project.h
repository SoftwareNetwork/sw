/*
 * SW - Build System and Package Manager
 * Copyright (C) 2016-2020 Egor Pugin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

//#include "dependency.h"
#include "yaml.h"

#include <sw/support/package_path.h>
#include <sw/support/source.h>
#include <sw/support/version.h>

#include <optional>

#define DEPENDENCIES_NODE "dependencies"
#define INCLUDE_DIRECTORIES_ONLY "include_directories_only"

namespace sw::cppan
{

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

#if 0

using Sources = std::set<String>;

enum class ProjectType
{
    None,
    Library,
    Executable,
    RootProject,
    Directory,
};

enum class ExecutableType
{
    Default,
    Win32,
};

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
    String include_script;
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
    //bool disabled = false;
    bool skip_on_server = false;
    bool create_default_api = false;
    String default_api_start;
    bool copy_to_output_dir = true;

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

    // for add_directories
    String subdir;

    // private data
private:
    // no files to compile
    std::optional<bool> header_only;

public:
    Project();
    Project(const PackagePath &root_project);

    void applyFlags(PackageFlag &flags) const;
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
    PackagePath root_project;

    const Files &getSources() const;
    PackagePath relative_name_to_absolute(const String &name);
};

using Projects = std::map<String, Project>;

void load_source_and_version(const yaml &root, Source &source, Version &version);

inline const auto bazel_filenames = { "BUILD", "BUILD.bazel" };

#endif

} // namespace sw::cppan
