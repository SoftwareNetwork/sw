// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "generator.h"

#include <sw/manager/package.h>

namespace sw
{

struct PackageId;
struct BuildSettings;

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
    using Directories = std::set<PackagePath>;

    std::map<String, PackagePathTree> tree;

    void add(const PackagePath &p);
    Directories getDirectories(const PackagePath &p = {});
};

struct XmlEmitter : primitives::Emitter
{
    std::stack<String> blocks;

    XmlEmitter(bool print_version = true);

    void beginBlock(const String &n, const std::map<String, String> &params = {}, bool empty = false);
    void beginBlockWithConfiguration(const String &n, const BuildSettings &s, std::map<String, String> params = {}, bool empty = false);
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

    void addProjectConfigurations(const Build &b);
    void addPropertyGroupConfigurationTypes(const Build &b);
    void addPropertyGroupConfigurationTypes(const Build &b, const PackageId &p);
    void addPropertyGroupConfigurationTypes(const Build &b, VSProjectType t);
    void addConfigurationType(VSProjectType t);

    void addPropertySheets(const Build &b);

    void printProject(
        const String &name, const PackageId &p, const Build &b, SolutionEmitter &ctx, Generator &g,
        PackagePathTree::Directories &parents, PackagePathTree::Directories &local_parents,
        const path &dir, const path &projects_dir
    );
};

struct SolutionEmitter : primitives::Emitter
{
    struct Project
    {
        String name;
        std::unique_ptr<SolutionEmitter> ctx;
        ProjectEmitter pctx;
        String solution_dir;

        Project()
        {
            ctx = std::make_unique<SolutionEmitter>();
        }
        ~Project()
        {
            if (ctx)
                ctx->parent_ = nullptr;
        }
    };

    using Base = primitives::Emitter;

    Version version;
    String all_build_name;
    String build_dependencies_name;
    PackageIdSet build_deps;
    std::unordered_map<String, String> uuids;
    std::map<String, Project> projects;
    const Project *first_project = nullptr;

    SolutionEmitter();

    void printVersion();

    void addDirectory(const String &display_name);
    void addDirectory(const InsecurePath &n, const String &display_name, const String &solution_dir = {});

    Project &addProject(VSProjectType type, const String &n, const String &solution_dir);
    void beginProject(VSProjectType type, const String &n, const path &dir, const String &solution_dir);
    void endProject();

    void beginBlock(const String &s);
    void endBlock(const String &s);

    void beginGlobal();
    void endGlobal();

    void beginGlobalSection(const String &name, const String &post);
    void endGlobalSection();

    void setSolutionConfigurationPlatforms(const Build &b);
    void addProjectConfigurationPlatforms(const Build &b, const String &prj, bool build = false);

    void beginProjectSection(const String &n, const String &disposition);
    void endProjectSection();

    void addKeyValue(const String &k, const String &v);
    String getStringUuid(const String &k) const;
    Text getText() const override;
    void materialize(const Build &b, const path &dir, GeneratorType t);

private:
    std::map<String, String> nested_projects;

    void printNestedProjects();
};

}
