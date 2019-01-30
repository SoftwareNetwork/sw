// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "options_cl.h"

#include <optional>
#include <vector>

namespace sw
{

namespace vs
{

struct SW_DRIVER_CPP_API ExceptionHandling
{
    bool SEH = false;               // a
    bool CPP = true;                // s
    bool ExternCMayThrow = false;   // c
    bool TerminationChecks = false; // r
    bool ClearFlag = false;         // -

    String getCommandLine() const;
};

using ExceptionHandlingVector = std::vector<ExceptionHandling>;

enum class MachineType
{
    ARM,
    ARM64,
    EBC,
    IA64,
    MIPS,
    MIPS16,
    MIPSFPU,
    MIPSFPU16,
    SH4,
    THUMB,
    X64,
    X86,
};

enum class RuntimeLibraryType
{
    MultiThreaded,
    MultiThreadedDebug,
    MultiThreadedDLL,
    MultiThreadedDLLDebug,

    MT = MultiThreaded,
    MTd = MultiThreadedDebug,
    MD = MultiThreadedDLL,
    MDd = MultiThreadedDLLDebug,
};

enum class DebugInformationFormatType
{
    None,
    ObjectFile,
    ProgramDatabase,
    ProgramDatabaseEditAndContinue,

    Z7 = ObjectFile,
    Zi = ProgramDatabase,
    ZI = ProgramDatabaseEditAndContinue,
};

enum class Subsystem
{
    Console,
    Windows,
    Native,
    EFI_Application,
    EFI_BootServiceDriver,
    EFI_ROM,
    EFI_RuntimeDriver,
    Posix
};

struct PrecompiledHeaderVs
{
    bool ignore = false;
    bool with_debug_info = false;
    std::optional<path> create;
    std::optional<path> use;

    Strings getCommandLine(::sw::builder::Command *c) const;
};

enum class ForceType
{
    Multiple,
    Unresolved,
};

struct Warnings
{
    bool DisableAll = false;
    bool EnableAll = false;
    int Level = 3;
    std::vector<int> Disable;
    std::map<int, std::vector<int>> DisableOnLevel;
    bool TreatAllWarningsAsError = false;
    std::vector<int> TreatAsError;
    std::vector<int> DisplayOnce;
    bool EnableOneLineDiagnostics = false;
};

struct Optimizations
{
    bool Disable = false;
    int Level = 2;
    bool SmallCode = false;
    bool FastCode = false;
};

namespace cs
{

enum class Target
{
    Console,
    Windows,
    Native,
    Library,
    Module,
    AppContainer,
    Winmdobj,
};

}

}

DECLARE_OPTION_SPECIALIZATION(vs::ExceptionHandlingVector);
DECLARE_OPTION_SPECIALIZATION(vs::MachineType);
DECLARE_OPTION_SPECIALIZATION(vs::RuntimeLibraryType);
DECLARE_OPTION_SPECIALIZATION(vs::DebugInformationFormatType);
DECLARE_OPTION_SPECIALIZATION(vs::Subsystem);
DECLARE_OPTION_SPECIALIZATION(vs::PrecompiledHeaderVs);
DECLARE_OPTION_SPECIALIZATION(vs::ForceType);
DECLARE_OPTION_SPECIALIZATION(vs::Warnings);
DECLARE_OPTION_SPECIALIZATION(vs::Optimizations);
DECLARE_OPTION_SPECIALIZATION(CPPLanguageStandard);

DECLARE_OPTION_SPECIALIZATION(vs::cs::Target);

Strings getCommandLineImplCPPLanguageStandardVS(const CommandLineOption<CPPLanguageStandard> &co, ::sw::builder::Command *c);

namespace rust
{

enum class CrateType
{
    bin,
    lib,
    rlib,
    dylib,
    cdylib,
    staticlib,
    proc_macro
};

}

DECLARE_OPTION_SPECIALIZATION(rust::CrateType);

}

#include "options_cl.generated.h"

namespace sw
{

struct SW_DRIVER_CPP_API RustCompilerOptions
{
    COMMAND_LINE_OPTION(CrateType, rust::CrateType)
    {
        cl::CommandFlag{ "-crate-type" },
            rust::CrateType::bin,
            cl::SeparatePrefix{},
    };

    COMMAND_LINE_OPTION(InputFile, path)
    {
        cl::InputDependency{},
    };

    COMMAND_LINE_OPTION(Output, path)
    {
        cl::CommandFlag{ "o" },
            cl::OutputDependency{},
    };
};
DECLARE_OPTION_SPECIALIZATION(RustCompilerOptions);

struct SW_DRIVER_CPP_API GoCompilerOptions
{
    COMMAND_LINE_OPTION(Command, String)
    {
        "build"s,
    };

    COMMAND_LINE_OPTION(Output, path)
    {
        cl::CommandFlag{ "o" },
            cl::OutputDependency{},
            cl::SeparatePrefix{},
    };

    COMMAND_LINE_OPTION(BuildMode, String)
    {
        cl::CommandFlag{ "buildmode=" },
            "default"s
    };

    COMMAND_LINE_OPTION(InputFiles, Files)
    {
        cl::InputDependency{},
    };
};
DECLARE_OPTION_SPECIALIZATION(GoCompilerOptions);

struct SW_DRIVER_CPP_API FortranCompilerOptions
{
    COMMAND_LINE_OPTION(Output, path)
    {
        cl::CommandFlag{ "o" },
            cl::OutputDependency{},
    };

    COMMAND_LINE_OPTION(InputFiles, Files)
    {
        cl::InputDependency{},
    };
};
DECLARE_OPTION_SPECIALIZATION(FortranCompilerOptions);

struct SW_DRIVER_CPP_API JavaCompilerOptions
{
    COMMAND_LINE_OPTION(OutputDir, path)
    {
        cl::CommandFlag{ "d" },
            cl::SeparatePrefix{},
    };

    COMMAND_LINE_OPTION(InputFiles, Files)
    {
        cl::InputDependency{},
    };
};
DECLARE_OPTION_SPECIALIZATION(JavaCompilerOptions);

struct SW_DRIVER_CPP_API KotlinCompilerOptions
{
    COMMAND_LINE_OPTION(InputFiles, Files)
    {
        cl::InputDependency{},
    };

    COMMAND_LINE_OPTION(IncludeRuntime, bool)
    {
        cl::CommandFlag{ "include-runtime" },
            true
    };

    COMMAND_LINE_OPTION(Output, path)
    {
        cl::CommandFlag{ "d" },
            cl::OutputDependency{},
            cl::SeparatePrefix{},
    };
};
DECLARE_OPTION_SPECIALIZATION(KotlinCompilerOptions);

struct SW_DRIVER_CPP_API DCompilerOptions
{
    COMMAND_LINE_OPTION(InputFiles, Files)
    {
        cl::InputDependency{},
    };

    COMMAND_LINE_OPTION(DoNotWriteObjectFiles, bool)
    {
        cl::CommandFlag{ "o-" },
    };

    COMMAND_LINE_OPTION(ObjectDir, path)
    {
        cl::CommandFlag{ "od=" },
    };

    COMMAND_LINE_OPTION(Output, path)
    {
        cl::CommandFlag{ "of=" },
            cl::OutputDependency{},
    };
};
DECLARE_OPTION_SPECIALIZATION(DCompilerOptions);

}
