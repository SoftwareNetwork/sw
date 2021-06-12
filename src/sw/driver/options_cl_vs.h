// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "options_cl.h"

#include <optional>
#include <vector>

namespace sw
{

//struct ProjectEmitter;

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
    ProgramDatabaseInObjectFile,
    ProgramDatabaseInSeparateFile,
    ProgramDatabaseEditAndContinue,

    Z7 = ProgramDatabaseInObjectFile,
    Zi = ProgramDatabaseInSeparateFile,
    ZI = ProgramDatabaseEditAndContinue,
};

enum class ForceType
{
    Multiple,
    Unresolved,
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
DECLARE_OPTION_SPECIALIZATION(vs::RuntimeLibraryType);
DECLARE_OPTION_SPECIALIZATION(vs::DebugInformationFormatType);
DECLARE_OPTION_SPECIALIZATION(vs::ForceType);
DECLARE_OPTION_SPECIALIZATION(vs::Optimizations);
DECLARE_OPTION_SPECIALIZATION(CLanguageStandard);
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

namespace gnu
{

struct Optimizations
{
    bool Disable = false;
    std::optional<int> Level;
    bool SmallCode = false;
    bool FastCode = false;
};

} // namespace gnu

DECLARE_OPTION_SPECIALIZATION(gnu::Optimizations);

namespace clang
{

enum class ArchType
{
    m32,
    m64,
};

} // namespace clang

DECLARE_OPTION_SPECIALIZATION(clang::ArchType);

}

#include <options_cl.generated.h>
