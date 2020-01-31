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

#include <sw/manager/package_path.h>
#include <sw/manager/version.h>

#include <primitives/emitter.h>
#include <primitives/filesystem.h>

namespace sw
{

struct SwBuild;

}

enum class GeneratorType
{
    // everything
    Batch,
    CMake,
    CompilationDatabase,
    Make,
    NMake,
    Ninja,
    RawBootstrapBuild,
    QMake,
    Shell,

    // sw
    SwExecutionPlan,
    SwBuildDescription, // simply BDesc?

    // IDE
    CodeBlocks,
    VisualStudio,
    Xcode,
    // qt creator?

    Max,
};

String toString(GeneratorType t);

enum class VsGeneratorType
{
    VisualStudio,
    VisualStudioNMake,
    //VisualStudioUtility,
    //VisualStudioNMakeAndUtility,
};

struct Options;

struct Generator
{
    const Options &options;

    Generator(const Options &options);
    virtual ~Generator() = default;

    virtual void generate(const sw::SwBuild &) = 0;
    static std::unique_ptr<Generator> create(const Options &);
    GeneratorType getType() const { return type; }

    path getRootDirectory(const sw::SwBuild &) const;
    virtual path getPathString() const;

private:
    GeneratorType type;
};

struct VSGenerator : Generator
{
    sw::Version vs_version;
    sw::Version toolset_version;
    path sln_root;
    VsGeneratorType vstype;
    sw::Version winsdk;
    bool add_overridden_packages = false;
    bool add_all_packages = false;

    using Generator::Generator;
    void generate(const sw::SwBuild &b) override;

    path getPathString() const override;
};

struct NinjaGenerator : Generator
{
    using Generator::Generator;
    void generate(const sw::SwBuild &) override;
};

struct CMakeGenerator : Generator
{
    using Generator::Generator;
    void generate(const sw::SwBuild &b) override;
};

struct MakeGenerator : Generator
{
    using Generator::Generator;
    void generate(const sw::SwBuild &b) override;
};

struct ShellGenerator : Generator
{
    bool batch = false;

    using Generator::Generator;
    void generate(const sw::SwBuild &b) override;
};

struct CompilationDatabaseGenerator : Generator
{
    using Generator::Generator;
    void generate(const sw::SwBuild &b) override;
};

struct SwExecutionPlanGenerator : Generator
{
    using Generator::Generator;
    void generate(const sw::SwBuild &b) override;
};

struct SwBuildDescriptionGenerator : Generator
{
    using Generator::Generator;
    void generate(const sw::SwBuild &b) override;
};

struct RawBootstrapBuildGenerator : Generator
{
    using Generator::Generator;
    void generate(const sw::SwBuild &b) override;
};

struct CodeBlocksGenerator : Generator
{
    using Generator::Generator;
    void generate(const sw::SwBuild &b) override;
};

struct XcodeGenerator : Generator
{
    using Generator::Generator;
    void generate(const sw::SwBuild &b) override;
};

String toPathString(GeneratorType Type);
