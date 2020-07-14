// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <sw/support/version.h>

#include <primitives/emitter.h>

struct Directory;
struct Project;
struct Solution;

// https://docs.microsoft.com/en-us/visualstudio/extensibility/internals/solution-dot-sln-file?view=vs-2019
struct SolutionEmitter : primitives::Emitter
{
    sw::Version version;

    SolutionEmitter();

    void printVersion();

    void addDirectory(const Directory &);

    void beginProject(const Project &);
    void endProject();

    void beginBlock(const String &s);
    void endBlock(const String &s);

    void beginGlobalSection(const String &name, const String &post);
    void endGlobalSection();

    void beginGlobal();
    void endGlobal();

    void setSolutionConfigurationPlatforms(const Solution &);
    void addProjectConfigurationPlatforms(const Project &, bool build);

    void addKeyValue(const String &k, const String &v);

    void beginProjectSection(const String &n, const String &disposition);
    void endProjectSection();
};

static const path vs_project_dir = "projects";
static const String vs_project_ext = ".vcxproj";
