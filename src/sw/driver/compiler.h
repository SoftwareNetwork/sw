// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "options.h"
#include "options_cl.h"
#include "options_cl_vs.h"
#include "program.h"
#include "types.h"

#include <sw/manager/version.h>

#include <primitives/exceptions.h>

#include <memory>

#define SW_COMMON_COMPILER_API                          \
    SW_DECLARE_PROGRAM_CLONE;                           \
    void prepareCommand1(const ::sw::Target &t) override

namespace sw
{

namespace builder
{
struct Command;
}

namespace driver
{
struct Command;
}

struct BuildSettings;
struct Build;
struct SwDriverContext;
struct Target;
struct NativeCompiledTarget;
struct NativeLinker;

/*enum class VisualStudioVersion
{
    Unspecified,

    //VS7 = 71,
    VS8 = 80,
    VS9 = 90,
    VS10 = 100,
    VS11 = 110,
    VS12 = 120,
    //VS13 = 130 was skipped
    VS14 = 140,
    VS15 = 150,
    VS16 = 160,
};*/

SW_DRIVER_CPP_API
void detectCompilers(Build &);

SW_DRIVER_CPP_API
const StringSet &getCppHeaderFileExtensions();

SW_DRIVER_CPP_API
const StringSet &getCppSourceFileExtensions();

SW_DRIVER_CPP_API
bool isCppHeaderFileExtension(const String &);

SW_DRIVER_CPP_API
bool isCppSourceFileExtensions(const String &);

// maybe add Object type?
struct VSInstance : ProgramGroup
{
    path root;
    Version version;

    // one installation may have more that one versions (tool sets)
    VersionSet cl_versions; // cl has 19.xx versions (19.15, 19.16, 19.20 etc.)
    VersionSet link_versions; // tools has 14.xx versions (14.15, 14.16, 14.20 etc.)

    using ProgramGroup::ProgramGroup;

    std::shared_ptr<Program> clone() const override { return std::make_shared<VSInstance>(*this); }
    Version &getVersion() override { return version; }

    void activate(Build &s) const override;
};

// toolchain

struct SW_DRIVER_CPP_API NativeToolchain
{
    struct SW_DRIVER_CPP_API SDK
    {
        // root to sdks
        //  example: c:\\Program Files (x86)\\Windows Kits
        path Root;

        // sdk dir in root
        // win: 7.0 7.0A, 7.1, 7.1A, 8, 8.1, 10 ...
        // osx: 10.12, 10.13, 10.14 ...
        // android: 1, 2, 3, ..., 28
        path Version; // make string?

        // windows10:
        // 10.0.10240.0, 10.0.17763.0 ...
        path BuildNumber;

        path getPath(const path &subdir = {}) const;
        String getWindowsTargetPlatformVersion() const;
        void setAndroidApiVersion(int v);

        bool operator<(const SDK &) const;
        bool operator==(const SDK &) const;
    };

    struct SDK SDK;

    // libc, libcpp
    // OS SDK (win sdk, macos sdk, linux headers etc.)
    //std::vector<NativeCompiledTarget*> ForcedDependencies;

    //std::shared_ptr<NativeLinker> Librarian;
    //std::shared_ptr<NativeLinker> Linker;

    // rc (resource compiler)
    // ar, more tools...
    // more native compilers (cuda etc.)
    ::sw::CompilerType CompilerType1 = CompilerType::UnspecifiedCompiler;
    //LinkerType LinkerType; // rename - use type from selected tool
    BuildLibrariesAs LibrariesType = LibraryType::Shared;
    ::sw::ConfigurationType ConfigurationType = ConfigurationType::Release;

    // win, vs
    bool MT = false;
    // toolset
    // win sdk
    // add XP support
    // more settings

    // misc
    //bool CopySharedLibraries = true;

    // service

    /// set on server to eat everything
    //bool AssignAll = false;

    // members
    //String getConfig() const;

    bool operator<(const NativeToolchain &) const;
    bool operator==(const NativeToolchain &) const;
};

// compilers

struct SW_DRIVER_CPP_API CompilerBaseProgram : FileToFileTransformProgram
{
    String Prefix;
    String Extension;

    using FileToFileTransformProgram::FileToFileTransformProgram;
    CompilerBaseProgram(const CompilerBaseProgram &);

    std::shared_ptr<builder::Command> prepareCommand(const Target &t);
    std::shared_ptr<builder::Command> getCommand(const Target &t);
    std::shared_ptr<builder::Command> getCommand() const override;
    std::shared_ptr<builder::Command> createCommand(const SwDriverContext &swctx);

protected:
    std::shared_ptr<driver::Command> cmd;
    bool prepared = false;

    virtual void prepareCommand1(const Target &t) = 0;
    virtual std::shared_ptr<driver::Command> createCommand1(const SwDriverContext &swctx) const;

private:
    std::shared_ptr<SourceFile> createSourceFile(const Target &t, const path &input) const override;
};

struct SW_DRIVER_CPP_API Compiler : CompilerBaseProgram
{
    using CompilerBaseProgram::CompilerBaseProgram;
    virtual ~Compiler() = default;
};

struct SW_DRIVER_CPP_API NativeCompiler : Compiler,
    NativeCompilerOptions//, OptionsGroup<NativeCompilerOptions>
{
    CompilerType Type = CompilerType::UnspecifiedCompiler;

    using Compiler::Compiler;
    virtual ~NativeCompiler() = default;

    virtual path getOutputFile() const = 0;
    virtual void setSourceFile(const path &input_file, path &output_file) = 0;
    String getObjectExtension(const struct OS &o) const;

protected:
    mutable Files dependencies;

private:
    std::shared_ptr<SourceFile> createSourceFile(const Target &t, const path &input) const override;
};

struct SW_DRIVER_CPP_API VisualStudio
{
    //VisualStudioVersion vs_version = VisualStudioVersion::Unspecified;
    String toolset;

    virtual ~VisualStudio() = default;
};

struct SW_DRIVER_CPP_API VisualStudioCompiler : VisualStudio,
    NativeCompiler,
    CommandLineOptions<VisualStudioCompilerOptions>
{
    using NativeCompiler::NativeCompiler;
    virtual ~VisualStudioCompiler() = default;

    using NativeCompilerOptions::operator=;

    SW_COMMON_COMPILER_API;

    void setOutputFile(const path &output_file);
    path getOutputFile() const override;
    void setSourceFile(const path &input_file, path &output_file) override;

protected:
    std::shared_ptr<driver::Command> createCommand1(const SwDriverContext &swctx) const override;

private:
    Version gatherVersion() const override { return Program::gatherVersion(file, "/?"); }
};

struct SW_DRIVER_CPP_API VisualStudioASMCompiler : VisualStudio, NativeCompiler,
    CommandLineOptions<VisualStudioAssemblerOptions>
{
    using NativeCompiler::NativeCompiler;
    virtual ~VisualStudioASMCompiler() = default;

    using NativeCompilerOptions::operator=;

    SW_COMMON_COMPILER_API;

    path getOutputFile() const override;
    void setSourceFile(const path &input_file, path &output_file) override;
    void setOutputFile(const path &output_file);

protected:
    std::shared_ptr<driver::Command> createCommand1(const SwDriverContext &swctx) const override;

private:
    Version gatherVersion() const override { return Program::gatherVersion(file, "/?"); }
};

struct SW_DRIVER_CPP_API Clang
{
};

struct SW_DRIVER_CPP_API ClangCompiler : Clang, NativeCompiler,
    CommandLineOptions<ClangOptions>
{
    using NativeCompiler::NativeCompiler;
    using NativeCompilerOptions::operator=;

    SW_COMMON_COMPILER_API;

    void setOutputFile(const path &output_file);
    void setSourceFile(const path &input_file, path &output_file) override;
    path getOutputFile() const override;

protected:
    std::shared_ptr<driver::Command> createCommand1(const SwDriverContext &swctx) const override;
};

struct SW_DRIVER_CPP_API ClangCl : Clang
{
    //Version vs_target_version;
};

struct SW_DRIVER_CPP_API ClangClCompiler : ClangCl,
    NativeCompiler,
    CommandLineOptions<VisualStudioCompilerOptions>,
    CommandLineOptions<ClangClOptions>
{
    using NativeCompiler::NativeCompiler;
    using NativeCompilerOptions::operator=;

    SW_COMMON_COMPILER_API;

    void setOutputFile(const path &output_file);
    void setSourceFile(const path &input_file, path &output_file) override;
    path getOutputFile() const override;

protected:
    std::shared_ptr<driver::Command> createCommand1(const SwDriverContext &swctx) const override;
};

struct SW_DRIVER_CPP_API GNU
{
};

struct SW_DRIVER_CPP_API GNUASMCompiler : GNU, NativeCompiler,
    CommandLineOptions<GNUAssemblerOptions>
{
    using NativeCompiler::NativeCompiler;
    virtual ~GNUASMCompiler() = default;

    using NativeCompilerOptions::operator=;

    SW_COMMON_COMPILER_API;

    void setSourceFile(const path &input_file, path &output_file) override;
    void setOutputFile(const path &output_file);
    path getOutputFile() const override;

protected:
    std::shared_ptr<driver::Command> createCommand1(const SwDriverContext &swctx) const override;
};

struct SW_DRIVER_CPP_API ClangASMCompiler : GNUASMCompiler
{
    using GNUASMCompiler::GNUASMCompiler;
    SW_DECLARE_PROGRAM_CLONE;
};

struct SW_DRIVER_CPP_API GNUCompiler : GNU, NativeCompiler,
    CommandLineOptions<GNUOptions>
{
    using NativeCompiler::NativeCompiler;
    using NativeCompilerOptions::operator=;

    SW_COMMON_COMPILER_API;

    void setOutputFile(const path &output_file);
    void setSourceFile(const path &input_file, path &output_file) override;
    path getOutputFile() const override;

protected:
    std::shared_ptr<driver::Command> createCommand1(const SwDriverContext &swctx) const override;
};

// linkers

struct SW_DRIVER_CPP_API Linker : CompilerBaseProgram
{
    using CompilerBaseProgram::CompilerBaseProgram;
    virtual ~Linker() = default;
};

struct SW_DRIVER_CPP_API NativeLinker : Linker,
    NativeLinkerOptions//, OptionsGroup<NativeLinkerOptions>
{
    LinkerType Type = LinkerType::UnspecifiedLinker;

    String Prefix;
    String Suffix;

    using Linker::Linker;

    virtual void setObjectFiles(const Files &files) = 0; // actually this is addObjectFiles()
    virtual void setInputLibraryDependencies(const FilesOrdered &files) {}
    virtual void setOutputFile(const path &out) = 0;
    virtual void setImportLibrary(const path &out) = 0;
    virtual void setLinkLibraries(const FilesOrdered &in) {}

    virtual path getOutputFile() const = 0;
    virtual path getImportLibrary() const = 0;

    FilesOrdered gatherLinkDirectories() const;
    FilesOrdered gatherLinkLibraries(bool system = false) const;
};

struct SW_DRIVER_CPP_API VisualStudioLibraryTool : VisualStudio,
    NativeLinker,
    CommandLineOptions<VisualStudioLibraryToolOptions>
{
    using NativeLinker::NativeLinker;

    void setObjectFiles(const Files &files) override;
    void setOutputFile(const path &out) override;
    void setImportLibrary(const path &out) override;

    path getOutputFile() const override;
    path getImportLibrary() const override;

protected:
    virtual void getAdditionalOptions(driver::Command *c) const = 0;

    void prepareCommand1(const Target &t) override;

private:
    Version gatherVersion() const override { return Program::gatherVersion(file, "/?"); }
};

struct SW_DRIVER_CPP_API VisualStudioLinker : VisualStudioLibraryTool,
    CommandLineOptions<VisualStudioLinkerOptions>
{
    using VisualStudioLibraryTool::VisualStudioLibraryTool;
    using NativeLinkerOptions::operator=;

    SW_DECLARE_PROGRAM_CLONE;
    void getAdditionalOptions(driver::Command *c) const override;
    void setInputLibraryDependencies(const FilesOrdered &files) override;

protected:
    void prepareCommand1(const Target &t) override;
};

struct SW_DRIVER_CPP_API VisualStudioLibrarian : VisualStudioLibraryTool,
    CommandLineOptions<VisualStudioLibrarianOptions>
{
    using VisualStudioLibraryTool::VisualStudioLibraryTool;
    using NativeLinkerOptions::operator=;

    SW_DECLARE_PROGRAM_CLONE;
    void getAdditionalOptions(driver::Command *c) const override;
};

struct SW_DRIVER_CPP_API GNULibraryTool : GNU,
    NativeLinker,
    CommandLineOptions<GNULibraryToolOptions>
{
    using NativeLinker::NativeLinker;

protected:
    virtual void getAdditionalOptions(driver::Command *c) const = 0;
};

struct SW_DRIVER_CPP_API GNULinker : GNULibraryTool,
    CommandLineOptions<GNULinkerOptions>
{
    bool use_start_end_groups = true;

    using GNULibraryTool::GNULibraryTool;
    using NativeLinkerOptions::operator=;

    void getAdditionalOptions(driver::Command *c) const override;

    void setInputLibraryDependencies(const FilesOrdered &files) override;
    void setObjectFiles(const Files &files) override;
    void setOutputFile(const path &out) override;
    void setImportLibrary(const path &out) override;
    void setLinkLibraries(const FilesOrdered &in) override;

    path getOutputFile() const override;
    path getImportLibrary() const override;

    SW_COMMON_COMPILER_API;
};

struct SW_DRIVER_CPP_API GNULibrarian : GNULibraryTool,
    CommandLineOptions<GNULibrarianOptions>
{
    using GNULibraryTool::GNULibraryTool;
    using NativeLinkerOptions::operator=;

    void getAdditionalOptions(driver::Command *c) const override;

    void setObjectFiles(const Files &files) override;
    void setOutputFile(const path &out) override;
    void setImportLibrary(const path &out) override;

    virtual path getOutputFile() const override;
    virtual path getImportLibrary() const override;

    SW_COMMON_COMPILER_API;

protected:
    Version gatherVersion() const override { return Program::gatherVersion(file, "-V"); }
};

// other tools

// win resources
struct SW_DRIVER_CPP_API RcTool :
    CompilerBaseProgram,
    CommandLineOptions<RcToolOptions>
{
    FilesOrdered idirs;

    using CompilerBaseProgram::CompilerBaseProgram;

    SW_COMMON_COMPILER_API;

    void setOutputFile(const path &output_file);
    void setSourceFile(const path &input_file);
    String getObjectExtension(const OS &) const { return ".res"; }

protected:
    Version gatherVersion() const override { return Program::gatherVersion(file, "/?"); }

private:
    std::shared_ptr<SourceFile> createSourceFile(const Target &t, const path &input) const override;
};

// C#

struct SW_DRIVER_CPP_API CSharpCompiler : Compiler
{
    using Compiler::Compiler;

    virtual void setOutputFile(const path &output_file) = 0;
    virtual void addSourceFile(const path &input_file) = 0;
};

// roslyn compiler?
struct SW_DRIVER_CPP_API VisualStudioCSharpCompiler :
    CSharpCompiler,
    CommandLineOptions<VisualStudioCSharpCompilerOptions>
{
    using CSharpCompiler::CSharpCompiler;

    SW_COMMON_COMPILER_API;

    void setOutputFile(const path &output_file) override;
    void addSourceFile(const path &input_file) override;

protected:
    Version gatherVersion() const override { return Program::gatherVersion(file, "/?"); }
};

struct SW_DRIVER_CPP_API RustCompiler : Compiler,
    CommandLineOptions<RustCompilerOptions>
{
    using Compiler::Compiler;

    SW_COMMON_COMPILER_API;

    void setOutputFile(const path &output_file);
    void setSourceFile(const path &input_file);
};

struct SW_DRIVER_CPP_API GoCompiler : Compiler,
    CommandLineOptions<GoCompilerOptions>
{
    using Compiler::Compiler;

    SW_COMMON_COMPILER_API;

    void setOutputFile(const path &output_file);
    void setSourceFile(const path &input_file);

private:
    Version gatherVersion() const override { return Program::gatherVersion(file, "version"); }
};

struct SW_DRIVER_CPP_API FortranCompiler : Compiler,
    CommandLineOptions<FortranCompilerOptions>
{
    using Compiler::Compiler;

    SW_COMMON_COMPILER_API;

    void setOutputFile(const path &output_file);
    void setSourceFile(const path &input_file);
};

struct SW_DRIVER_CPP_API JavaCompiler : Compiler,
    CommandLineOptions<JavaCompilerOptions>
{
    using Compiler::Compiler;

    SW_COMMON_COMPILER_API;

    void setOutputDir(const path &output_dir);
    void setSourceFile(const path &input_file);

private:
    Version gatherVersion() const override { return Program::gatherVersion(file, "-version", "(\\d+)\\.(\\d+)\\.(\\d+)(_(\\d+))?"); }
};

struct SW_DRIVER_CPP_API KotlinCompiler : Compiler,
    CommandLineOptions<KotlinCompilerOptions>
{
    using Compiler::Compiler;

    SW_COMMON_COMPILER_API;

    void setOutputFile(const path &output_file);
    void setSourceFile(const path &input_file);

private:
    Version gatherVersion() const override { return Program::gatherVersion(file, "-version"); }
};

struct SW_DRIVER_CPP_API DCompiler : Compiler,
    CommandLineOptions<DCompilerOptions>
{
    using Compiler::Compiler;

    SW_COMMON_COMPILER_API;

    void setOutputFile(const path &output_file);
    void setObjectDir(const path &dir);
    void setSourceFile(const path &input_file);
};

// TODO: compiled
// VB, VB.NET, Obj-C (check work), Pascal (+Delphi?), swift, dart, cobol, lisp, ada, haskell, F#, erlang

// TODO: interpreted
// python, js, php, R, ruby, matlab, perl, lua,

// TODO (other):
// Groovy, scala, prolog, apex, julia, clojure, bash

/*
How to add new lang:
- Add compiler
- Add 'void detectXCompilers(struct Solution &s);' function
- Call it from 'detectCompilers()'
- Add compiler options
- Add targets
- Add source file
- Add language
- Activate language (program) in solution
*/

}
