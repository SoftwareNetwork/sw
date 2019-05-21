// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
    CreateTargetInitialized,
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

    Build,
    Solution,

    Project, // explicitly created
    Directory, // implicitly created?

    NativeLibrary,
    NativeStaticLibrary,
    NativeSharedLibrary,
    NativeExecutable,

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
