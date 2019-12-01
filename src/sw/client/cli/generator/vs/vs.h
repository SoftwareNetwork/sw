/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2019 Egor Pugin
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

#include <sw/core/target.h>
#include <sw/manager/package.h>

#include <primitives/command.h>

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

struct File
{

};

struct Directory
{
    String name;
    String directory; // parent
    String uuid;
    Files files;
    VSProjectType type = VSProjectType::Directory;
    const VSGenerator *g = nullptr;

    Directory(const String &name);
};

struct ProjectData
{
    const sw::ITarget *target = nullptr;
    primitives::Command *main_command = nullptr;
    VSProjectType type = VSProjectType::Directory;
};

struct Project : Directory
{
    // settings
    std::set<const Project *> dependencies;
    Settings settings;
    std::map<sw::TargetSettings, ProjectData> data;
    bool build = false;

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
    void printProperties(ProjectEmitter &, const primitives::Command &, const Properties &props = {}) const;
};

struct Solution
{
    std::map<String, Directory> directories;
    std::map<String, Project> projects;
    const Project *first_project = nullptr;
    Settings settings;

    void emit(const VSGenerator &) const;

    const Settings &getSettings() const { return settings; }

private:
    void emitDirectories(SolutionEmitter &) const;
    void emitProjects(const path &root, SolutionEmitter &) const;
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
