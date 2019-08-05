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

namespace sw
{

struct PackageId;
struct SwBuild;
struct BuildSettings;

}

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
    String directory;
    String uuid;
    Files files;
    VSProjectType type = VSProjectType::Directory;
    const VSGenerator *g = nullptr;

    Directory() = default;
    Directory(const String &name);
};

struct Project : Directory
{
    // settings
    std::set<const Project *> dependencies;
    Settings settings;

    Project() = default;
    Project(const String &name);

    void emit(SolutionEmitter &) const;
    void emit(const VSGenerator &) const;
    void emitProject(const VSGenerator &) const;
    void emitFilters(const VSGenerator &) const;

    const Settings &getSettings() const { return settings; }
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

String get_project_configuration(const sw::BuildSettings &s);
