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

#include <sw/manager/version.h>

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

static path vs_project_dir = "projects";
static String vs_project_ext = ".vcxproj";
