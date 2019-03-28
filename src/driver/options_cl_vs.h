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

struct ProjectContext;

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

#include "options_cl.generated.h"
