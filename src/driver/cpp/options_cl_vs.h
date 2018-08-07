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

Strings getCommandLineImplCPPLanguageStandardVS(const CommandLineOption<CPPLanguageStandard> &co, ::sw::builder::Command *c);

struct SW_DRIVER_CPP_API VisualStudioCommonOptions
{
    COMMAND_LINE_OPTION(Nologo, bool)
    {
        cl::CommandFlag{ "nologo" }, true,
    };
};

struct SW_DRIVER_CPP_API VisualStudioCommonCompilerOptions : VisualStudioCommonOptions
{
    COMMAND_LINE_OPTION(CompileWithoutLinking, bool)
    {
        cl::CommandFlag{ "c" }, true,
    };

    COMMAND_LINE_OPTION(PreprocessToStdout, bool)
    {
        cl::CommandFlag{ "EP" },
    };

    COMMAND_LINE_OPTION(ObjectFile, path)
    {
        cl::CommandFlag{ "Fo" },
            cl::OutputDependency{},
    };
};

struct SW_DRIVER_CPP_API VisualStudioAssemblerOptions : VisualStudioCommonCompilerOptions
{
    COMMAND_LINE_OPTION(PreserveSymbolCase, bool)
    {
        cl::CommandFlag{ "Cx" },
    };

    COMMAND_LINE_OPTION(SafeSEH, bool)
    {
        cl::CommandFlag{ "safeseh" }
    };

    // goes last
    COMMAND_LINE_OPTION(InputFile, path)
    {
        cl::InputDependency{},
    };
};
DECLARE_OPTION_SPECIALIZATION(VisualStudioAssemblerOptions);

// cl.exe
// https://docs.microsoft.com/en-us/cpp/build/reference/compiler-options-listed-alphabetically
struct SW_DRIVER_CPP_API VisualStudioCompilerOptions : VisualStudioCommonCompilerOptions
{
    COMMAND_LINE_OPTION(BigObj, bool)
    {
        cl::CommandFlag{ "bigobj" },
    };

    COMMAND_LINE_OPTION(CSourceFile, path)
    {
        cl::CommandFlag{ "Tc" },
            cl::InputDependency{},
    };

    COMMAND_LINE_OPTION(CPPStandard, CPPLanguageStandard)
    {
        cl::CommandLineFunction<CPPLanguageStandard>{&getCommandLineImplCPPLanguageStandardVS},
    };

    COMMAND_LINE_OPTION(CPPSourceFile, path)
    {
        cl::CommandFlag{ "Tp" },
            cl::InputDependency{},
    };

    // merge with CompileAsCPP
    COMMAND_LINE_OPTION(CompileAsC, bool)
    {
        cl::CommandFlag{ "TC" },
    };

    COMMAND_LINE_OPTION(CompileAsCPP, bool)
    {
        cl::CommandFlag{ "TP" },
    };

    COMMAND_LINE_OPTION(DebugInformationFormat, vs::DebugInformationFormatType)
    {
        vs::DebugInformationFormatType::ProgramDatabase,
    };

    COMMAND_LINE_OPTION(ExceptionHandlingModel, vs::ExceptionHandlingVector)
    {
        vs::ExceptionHandlingVector{ vs::ExceptionHandling{} }
    };

    COMMAND_LINE_OPTION(ForcedIncludeFiles, FilesOrdered)
    {
        cl::CommandFlag{ "FI" },
            cl::CommandFlagBeforeEachValue{}
    };

    COMMAND_LINE_OPTION(ForceSynchronousPDBWrites, bool)
    {
        cl::CommandFlag{ "FS" },
            true
    };

    COMMAND_LINE_OPTION(Optimizations, vs::Optimizations);

    COMMAND_LINE_OPTION(PDBFilename, path)
    {
        cl::CommandFlag{ "Fd" },
            cl::IntermediateFile{},
    };

    COMMAND_LINE_OPTION(PrecompiledHeaderFilename, path)
    {
        cl::CommandFlag{ "Fp" }
    };

    COMMAND_LINE_OPTION(PrecompiledHeader, vs::PrecompiledHeaderVs);

    COMMAND_LINE_OPTION(PreprocessToFile, bool)
    {
        cl::CommandFlag{ "P" },
    };

    COMMAND_LINE_OPTION(RuntimeLibrary, vs::RuntimeLibraryType)
    {
        vs::RuntimeLibraryType::MultiThreadedDLL
    };

    COMMAND_LINE_OPTION(SerializedPDBWrites, bool)
    {
        cl::CommandFlag{ "FS" },
    };

    COMMAND_LINE_OPTION(ShowIncludes, bool)
    {
        cl::CommandFlag{ "showIncludes" },
            true
    };

    COMMAND_LINE_OPTION(UTF8, bool)
    {
        cl::CommandFlag{ "utf-8" },
            true
    };

    COMMAND_LINE_OPTION(Warnings, vs::Warnings);

    // goes last
    COMMAND_LINE_OPTION(InputFile, path)
    {
        cl::InputDependency{},
    }; // for TC/TP
};
DECLARE_OPTION_SPECIALIZATION(VisualStudioCompilerOptions);

// common for lib.exe and link.exe
// https://docs.microsoft.com/en-us/cpp/build/reference/linker-options
struct SW_DRIVER_CPP_API VisualStudioLibraryToolOptions : VisualStudioCommonOptions
{
    COMMAND_LINE_OPTION(InputFiles, Files)
    {
        cl::InputDependency{},
    };

    // these are ordered
    COMMAND_LINE_OPTION(LinkDirectories, FilesOrdered)
    {
        cl::CommandFlag{ "LIBPATH:" },
            cl::CommandFlagBeforeEachValue{},
    };

    //COMMAND_LINE_OPTION(LinkLibraries, FilesOrdered){ cl::InputDependency{}, };

    //implement target os
    COMMAND_LINE_OPTION(Machine, vs::MachineType)
    {
        cl::CommandFlag{ "MACHINE:" },
            vs::MachineType::X64
    };

    COMMAND_LINE_OPTION(DefinitionFile, path)
    {
        cl::CommandFlag{ "DEF:" },
            cl::InputDependency{},
    };

    COMMAND_LINE_OPTION(ImportLibrary, path)
    {
        cl::CommandFlag{ "IMPLIB:" },
            cl::IntermediateFile{},
    };

    COMMAND_LINE_OPTION(Output, path)
    {
        cl::CommandFlag{ "OUT:" },
            cl::OutputDependency{},
    };
};

// link.exe
// https://docs.microsoft.com/en-us/cpp/build/reference/linker-options
struct SW_DRIVER_CPP_API VisualStudioLinkerOptions
{
    COMMAND_LINE_OPTION(Dll, bool)
    {
        cl::CommandFlag{ "DLL" },
            cl::ConfigVariable{},
            false,
    };

    COMMAND_LINE_OPTION(DelayLoadDlls, FilesOrdered)
    {
        cl::CommandFlag{ "DELAYLOAD:" },
            cl::CommandFlagBeforeEachValue{}
    };

    COMMAND_LINE_OPTION(GenerateDebugInfo, bool)
    {
        cl::CommandFlag{ "DEBUG" },
    };

    COMMAND_LINE_OPTION(Force, vs::ForceType)
    {
        cl::CommandFlag{ "FORCE:" },
    };

    COMMAND_LINE_OPTION(PDBFilename, path)
    {
        cl::CommandFlag{ "PDB:" },
            cl::OutputDependency{},
    };

    COMMAND_LINE_OPTION(NoEntry, bool)
    {
        cl::CommandFlag{ "NOENTRY" }
    };

    COMMAND_LINE_OPTION(Subsystem, vs::Subsystem)
    {
        cl::CommandFlag{ "SUBSYSTEM:" }
    };
};
DECLARE_OPTION_SPECIALIZATION(VisualStudioLinkerOptions);

// lib.exe
struct SW_DRIVER_CPP_API VisualStudioLibrarianOptions
{
};

}
