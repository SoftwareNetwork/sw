// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

namespace sw
{

// ? (re)move?
enum class TargetScope
{
    Analyze,
    Benchmark,
    Build,
    Coverage,
    Documentation,
    Example,
    Format,
    Helper, // same as tool?
    Profile,
    Sanitize,
    Tool,
    Test,
    UnitTest,
    Valgrind,
};

enum class CallbackType
{
    CreateTarget,
    BeginPrepare,
    EndPrepare,

    //    std::vector<TargetEvent> PreBuild;
    // addCustomCommand()?
    // preBuild?
    // postBuild?
    // postLink?
};

// passed (serialized) via strings
enum class TargetType : int
{
    Unspecified,

    Project, // explicitly created
    Directory, // implicitly created?

    NativeLibrary,
    NativeHeaderOnlyLibrary,
    NativeStaticLibrary,
    NativeSharedLibrary,
    NativeObjectLibrary,
    NativeExecutable,

    // remove below?
    CSharpLibrary,
    CSharpExecutable,

    RustLibrary,
    RustExecutable,

    GoLibrary,
    GoExecutable,

    FortranLibrary,
    FortranExecutable,

    // add/replace with java jar?
    JavaLibrary,
    JavaExecutable,

    // add/replace with java jar?
    KotlinLibrary,
    KotlinExecutable,

    DLibrary,
    DStaticLibrary,
    DSharedLibrary,
    DExecutable,
};

// enforcement rules apply to target to say how many checks it should perform
enum class EnforcementType
{
    CheckFiles,
    CheckRegexes,
};

} // namespace sw
