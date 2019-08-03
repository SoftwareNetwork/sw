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

#include "generator.h"

#include <sw/core/target.h>
#include <sw/manager/package.h>

namespace sw
{

struct PackageId;
struct SwBuild;
struct BuildSettings;

}

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

struct PackagePathTree
{
    using Directories = std::set<sw::PackagePath>;

    std::map<String, PackagePathTree> tree;

    void add(const sw::PackagePath &p);
    Directories getDirectories(const sw::PackagePath &p = {});
};

struct XmlEmitter : primitives::Emitter
{
    std::stack<String> blocks;

    XmlEmitter(bool print_version = true);

    void beginBlock(const String &n, const std::map<String, String> &params = {}, bool empty = false);
    void beginBlockWithConfiguration(const String &n, const sw::BuildSettings &s, std::map<String, String> params = {}, bool empty = false);
    void endBlock(bool text = false);
    void addBlock(const String &n, const String &v, const std::map<String, String> &params = {});

protected:
    void beginBlock1(const String &n, const std::map<String, String> &params = {}, bool empty = false);
    void endBlock1(bool text = false);
};

struct FiltersEmitter : XmlEmitter
{
    void beginProject();
    void endProject();
};

struct SolutionEmitter;

struct ProjectEmitter : XmlEmitter
{
    SolutionEmitter *parent = nullptr;
    std::set<String> deps;
    VSProjectType ptype;

    void beginProject();
    void endProject();

    void addProjectConfigurations(const sw::SwBuild &b);
    void addPropertyGroupConfigurationTypes(const sw::SwBuild &b);
    void addPropertyGroupConfigurationTypes(const sw::SwBuild &b, const sw::PackageId &p);
    void addPropertyGroupConfigurationTypes(const sw::SwBuild &b, VSProjectType t);
    void addConfigurationType(VSProjectType t);

    void addPropertySheets(const sw::SwBuild &b);

    void printProject(
        const String &name, const sw::PackageId &p, const sw::SwBuild &b, SolutionEmitter &ctx, Generator &g,
        PackagePathTree::Directories &parents, PackagePathTree::Directories &local_parents,
        const path &dir, const path &projects_dir
    );

private:
    SolutionEmitter &getParent() const;
    const Settings &getSettings() const;
};

struct SolutionEmitter : primitives::Emitter
{
    struct Project
    {
        String name;
        SolutionEmitter *ctx = nullptr;
        ProjectEmitter pctx;
        String solution_dir;
    };

    using Base = primitives::Emitter;

    sw::Version version;
    String all_build_name;
    String build_dependencies_name;
    sw::PackageIdSet build_deps;
    std::unordered_map<String, String> uuids;
    std::map<String, Project> projects;
    const Project *first_project = nullptr;
    Files visualizers;
    Settings settings;

    SolutionEmitter();

    void printVersion();

    const Settings &getSettings() const;

    SolutionEmitter &addDirectory(const String &display_name);
    SolutionEmitter &addDirectory(const sw::InsecurePath &n, const String &display_name, const String &solution_dir = {});

    Project &addProject(VSProjectType type, const String &n, const String &solution_dir);
    void beginProject(VSProjectType type, const String &n, const path &dir, const String &solution_dir);
    void endProject();

    void beginBlock(const String &s);
    void endBlock(const String &s);

    void beginGlobal();
    void endGlobal();

    void beginGlobalSection(const String &name, const String &post);
    void endGlobalSection();

    void setSolutionConfigurationPlatforms(const sw::SwBuild &b);
    void addProjectConfigurationPlatforms(const sw::SwBuild &b, const String &prj, bool build = false);

    void beginProjectSection(const String &n, const String &disposition);
    void endProjectSection();

    void addKeyValue(const String &k, const String &v);
    String getStringUuid(const String &k) const;
    Text getText() const override;
    void materialize(const sw::SwBuild &b, const path &dir, GeneratorType t);

private:
    std::map<String, String> nested_projects;

    void printNestedProjects();
};

struct File
{

};

struct Project
{
    Files files;
    // settings
    std::set<const Project *> dependencies;
};

struct Solution
{
    std::vector<Project> directories;
    std::map<String, Project> projects;
    const Project *first_project = nullptr;
};
