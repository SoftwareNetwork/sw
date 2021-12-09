// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <sw/support/package_path.h>
#include <sw/support/version.h>

#include <primitives/emitter.h>
#include <primitives/filesystem.h>

namespace sw
{

struct SwBuild;

}

enum class GeneratorType
{
#define GENERATOR(x,y) x,
#include "generator.inl"
#undef GENERATOR
};

struct GeneratorDescription
{
    GeneratorType type;
    String name;
    String path_string;
    StringSet allowed_names;
};

SW_CLIENT_COMMON_API
const std::vector<GeneratorDescription> &getGenerators();

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
    enum CompilerType
    {
        MSVC,
        ClangCl,
        Clang,
    };

    const sw::SwBuild *b = nullptr;
    CompilerType compiler_type = MSVC;
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
    bool cmake_symlink = false;

    using Generator::Generator;
    void generate(const sw::SwBuild &b) override;
};

struct FastBuildGenerator : Generator
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
    bool allow_empty_file_directive = false;
    bool local_targets_only = false;
    bool compdb_symlink = false;
    bool compdb_clion = false;

    using Generator::Generator;
    void generate(const sw::SwBuild &b) override;

    path getPathString() const override;
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
