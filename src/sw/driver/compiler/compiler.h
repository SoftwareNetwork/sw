// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "base.h"
#include "../options_cl_vs.h"

#define SW_COMMON_COMPILER_API                          \
    SW_DECLARE_PROGRAM_CLONE;                           \
    void prepareCommand1(const ::sw::Target &t) override

namespace sw
{

struct SW_DRIVER_CPP_API VisualStudioCompiler
    : NativeCompiler
    , CommandLineOptions<VisualStudioCompilerOptions>
{
    using NativeCompiler::NativeCompiler;
    virtual ~VisualStudioCompiler() = default;

    using NativeCompilerOptions::operator=;

    SW_COMMON_COMPILER_API;

    void setOutputFile(const path &output_file);
    void setSourceFile(const path &input_file, const path &output_file) override;
};

struct SW_DRIVER_CPP_API VisualStudioASMCompiler
    : NativeCompiler
    , CommandLineOptions<VisualStudioAssemblerOptions>
{
    using NativeCompiler::NativeCompiler;
    virtual ~VisualStudioASMCompiler() = default;

    using NativeCompilerOptions::operator=;

    SW_COMMON_COMPILER_API;

    void setSourceFile(const path &input_file, const path &output_file) override;
    void setOutputFile(const path &output_file);
};

struct SW_DRIVER_CPP_API ClangCompiler
    : NativeCompiler
    , CommandLineOptions<ClangOptions>
{
    bool appleclang = false;

    using NativeCompiler::NativeCompiler;
    using NativeCompilerOptions::operator=;

    SW_COMMON_COMPILER_API;

    void setOutputFile(const path &output_file);
    void setSourceFile(const path &input_file, const path &output_file) override;
};

struct SW_DRIVER_CPP_API ClangClCompiler
    : NativeCompiler
    , CommandLineOptions<VisualStudioCompilerOptions>
    , CommandLineOptions<ClangClOptions>
{
    using NativeCompiler::NativeCompiler;
    using NativeCompilerOptions::operator=;

    SW_COMMON_COMPILER_API;

    void setOutputFile(const path &output_file);
    void setSourceFile(const path &input_file, const path &output_file) override;
};

struct SW_DRIVER_CPP_API GNUASMCompiler
    : NativeCompiler
    , CommandLineOptions<GNUAssemblerOptions>
{
    using NativeCompiler::NativeCompiler;
    virtual ~GNUASMCompiler() = default;

    using NativeCompilerOptions::operator=;

    SW_COMMON_COMPILER_API;

    void setSourceFile(const path &input_file, const path &output_file) override;
    void setOutputFile(const path &output_file);
};

struct SW_DRIVER_CPP_API ClangASMCompiler : GNUASMCompiler
{
    using GNUASMCompiler::GNUASMCompiler;
    SW_DECLARE_PROGRAM_CLONE;
};

struct SW_DRIVER_CPP_API GNUCompiler
    : NativeCompiler
    , CommandLineOptions<GNUOptions>
{
    using NativeCompiler::NativeCompiler;
    using NativeCompilerOptions::operator=;

    SW_COMMON_COMPILER_API;

    void setOutputFile(const path &output_file);
    void setSourceFile(const path &input_file, const path &output_file) override;
};

// linkers

struct SW_DRIVER_CPP_API VisualStudioLibraryTool
    : NativeLinker
    , CommandLineOptions<VisualStudioLibraryToolOptions>
{
    VisualStudioLibraryTool()
    {
        Type = LinkerType::MSVC;
    }

    void setObjectFiles(const FilesOrdered &files) override;
    void setOutputFile(const path &out) override;
    void setImportLibrary(const path &out) override;

    path getOutputFile() const override;
    path getImportLibrary() const override;

protected:
    virtual void getAdditionalOptions(driver::Command *c) const = 0;

    void prepareCommand1(const Target &t) override;
};

struct SW_DRIVER_CPP_API VisualStudioLinker : VisualStudioLibraryTool,
    CommandLineOptions<VisualStudioLinkerOptions>
{
    using VisualStudioLibraryTool::VisualStudioLibraryTool;
    using NativeLinkerOptions::operator=;

    SW_DECLARE_PROGRAM_CLONE;
    void getAdditionalOptions(driver::Command *c) const override;

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

struct SW_DRIVER_CPP_API GNULibraryTool
    : NativeLinker
    , CommandLineOptions<GNULibraryToolOptions>
{
    GNULibraryTool()
    {
        Type = LinkerType::GNU;
    }

protected:
    virtual void getAdditionalOptions(driver::Command *c) const = 0;
};

// we invoke linker via driver (gcc/clang)
// so linker options are prefixed with -Wl,
struct SW_DRIVER_CPP_API GNULinker : GNULibraryTool,
    CommandLineOptions<GNULinkerOptions>
{
    bool use_start_end_groups = true;

    using GNULibraryTool::GNULibraryTool;
    using NativeLinkerOptions::operator=;

    void getAdditionalOptions(driver::Command *c) const override;

    void setObjectFiles(const FilesOrdered &files) override;
    void setOutputFile(const path &out) override;
    void setImportLibrary(const path &out) override;

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

    void setObjectFiles(const FilesOrdered &files) override;
    void setOutputFile(const path &out) override;
    void setImportLibrary(const path &out) override;

    virtual path getOutputFile() const override;
    virtual path getImportLibrary() const override;

    SW_COMMON_COMPILER_API;
};

// Ada

struct SW_DRIVER_CPP_API AdaCompiler : Compiler,
    CommandLineOptions<AdaCompilerOptions>
{
    using Compiler::Compiler;

    SW_COMMON_COMPILER_API;

    void setOutputFile(const path &output_file);
    void addSourceFile(const path &input_file);
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
};

struct SW_DRIVER_CPP_API KotlinCompiler : Compiler,
    CommandLineOptions<KotlinCompilerOptions>
{
    using Compiler::Compiler;

    SW_COMMON_COMPILER_API;

    void setOutputFile(const path &output_file);
    void setSourceFile(const path &input_file);
};

struct SW_DRIVER_CPP_API DCompiler : NativeLinker,
    CommandLineOptions<DLinkerOptions>
{
    using NativeLinker::NativeLinker;

    SW_COMMON_COMPILER_API;

    void setObjectDir(const path &dir);
    void setSourceFile(const path &input_file);
    void setObjectFiles(const FilesOrdered &files) override {}

    path getOutputFile() const override;
    void setOutputFile(const path &out) override;

    path getImportLibrary() const override { return {}; }
    void setImportLibrary(const path &out) override {}
};

// Pascal

struct SW_DRIVER_CPP_API PascalCompiler : Compiler,
    CommandLineOptions<PascalCompilerOptions>
{
    using Compiler::Compiler;

    SW_COMMON_COMPILER_API;

    void setOutputFile(const path &output_file);
    void addSourceFile(const path &input_file);
};

// Vala

struct SW_DRIVER_CPP_API ValaCompiler : Compiler,
    CommandLineOptions<ValaOptions>
{
    using Compiler::Compiler;

    SW_COMMON_COMPILER_API;
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
