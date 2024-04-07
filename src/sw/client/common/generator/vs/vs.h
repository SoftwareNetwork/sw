// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <sw/builder/command.h>
#include <sw/core/target.h>
#include <sw/manager/package.h>

#include <functional>
#include <optional>
#include <unordered_set>

namespace sw
{

struct PackageId;
struct SwBuild;
struct BuildSettings;
struct ITarget;

}

struct ProjectEmitter;
struct SolutionEmitter;
struct VSGenerator;

using Settings = std::set<sw::TargetSettings>;

enum class VSProjectType
{
    Directory,
    Makefile,
    Application,
    DynamicLibrary,
    StaticLibrary,
    Utility,
};

struct icmp
{
    struct icasecmp
    {
        bool operator()(const char &c1, const char &c2) const
        {
            return tolower(c1) < tolower(c2);
        }
    };
    bool operator()(const std::string &s1, const std::string &s2) const
    {
        return std::lexicographical_compare(s1.begin(), s1.end(),
            s2.begin(), s2.end(),
            icasecmp());
    }
};

struct FileWithFilter
{
    path p;
    path filter; // (dir)
    // command
    // is generated

    FileWithFilter(const path &p, const path &f = {}) : p(p), filter(f) {}

    bool operator==(const FileWithFilter &rhs) const
    {
        return std::tie(p) == std::tie(rhs.p);
    }
};

namespace std
{

template<> struct hash<::FileWithFilter>
{
    size_t operator()(const ::FileWithFilter &f) const
    {
        return std::hash<::path>()(f.p);
    }
};

}

using FilesWithFilter = std::unordered_set<FileWithFilter>;

struct Rule
{
    String name;
    String message;
    String command;
    Files inputs;
    Files outputs;
    bool verify_inputs_and_outputs_exist = true;
};

struct BuildEvent
{
    String command;
};

using DirectoryPath = String;

struct Directory;

struct CommonProjectData
{
    String name;
    String visible_name;
    Directory *directory = nullptr; // parent
    String uuid;
    VSProjectType type = VSProjectType::Directory;
    const VSGenerator *g = nullptr;

    FilesWithFilter files;

    CommonProjectData(const String &name);

    String getVisibleName() const;
};

struct Directory : CommonProjectData
{
    using CommonProjectData::CommonProjectData;
};

using Command = const sw::builder::Command *;

// per config data
struct ProjectData
{
    const sw::ITarget *target = nullptr;
    Command main_command = nullptr;
    Command pre_link_command = nullptr;
    VSProjectType type = VSProjectType::Directory;
    std::unordered_set<Command> custom_rules;
    std::vector<Rule> custom_rules_manual; // not commands
    std::unordered_map<Command, path> build_rules;
    std::unordered_map<path, path> rewrite_dirs;
    std::optional<BuildEvent> pre_build_event;
    std::set<const sw::ITarget *> dependencies; // per config deps
    path binary_dir;
    path binary_private_dir;
    String nmake_build;
    String nmake_clean;
    String nmake_rebuild;
};

struct Project : CommonProjectData
{
    // settings
    std::set<const Project *> dependencies; // solution deps
    Settings settings;
    std::map<sw::TargetSettings, ProjectData> data;
    bool build = false;
    path source_dir;
    mutable StringSet filters; // dirs

    Project(const String &name);

    void emit(SolutionEmitter &) const;
    void emit(const VSGenerator &) const;

    const Settings &getSettings() const { return settings; }
    ProjectData &getData(const sw::TargetSettings &);
    const ProjectData &getData(const sw::TargetSettings &) const;

private:
    struct Properties
    {
        StringSet exclude_flags;
        StringSet exclude_exts;
    };

    void emitProject(const VSGenerator &) const;
    void emitFilters(const VSGenerator &) const;
    void emitUserSettings(const VSGenerator &) const;
    std::map<String, String> printProperties(const sw::builder::Command &, const Properties &exclude_props) const;

    static String get_flag_table(const primitives::Command &, bool throw_on_error = true);
};

struct Solution
{
    std::map<DirectoryPath, Directory> directories;
    std::map<String, Project, icmp> projects;
    const Project *first_project = nullptr;
    Settings settings;

    void emit(const VSGenerator &, const String &slnfn) const;

    const Settings &getSettings() const { return settings; }

private:
    void emitDirectories(SolutionEmitter &) const;
    void emitProjects(const path &root, SolutionEmitter &) const;
};

struct PackagePathTree
{
    using Directories = std::set<sw::PackagePath>;

    std::map<String, PackagePathTree> tree;
    sw::PackageIdSet projects;

    void add(const sw::PackageId &);
    Directories getDirectories(const sw::PackagePath &p = {});

private:
    void add(const sw::PackagePath &, const sw::PackageId &project);
};

enum class FlagTableFlags
{
    Empty                   = 0x00,
    UserValue               = 0x01,
    SemicolonAppendable     = 0x02,
    UserRequired            = 0x04,
    UserIgnored             = 0x08,
    UserFollowing           = 0x10,
    Continue                = 0x20,
    CaseInsensitive         = 0x40,
    SpaceAppendable         = 0x80,
};
ENABLE_ENUM_CLASS_BITMASK(FlagTableFlags);

struct FlagTableData
{
    String name;
    String argument;
    String comment;
    String value;
    FlagTableFlags flags = FlagTableFlags::Empty;
};

struct FlagTable
{
    std::map<String /* flag name */, FlagTableData> table;
    std::unordered_map<String, FlagTableData> ftable;
};

using FlagTables = std::map<String /* command name */, FlagTable>;

String get_project_configuration(const sw::BuildSettings &s);
