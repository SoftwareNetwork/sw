/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2020 Egor Pugin
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

    Project, // explicitly created
    Directory, // implicitly created?

    NativeLibrary,
    NativeHeaderOnlyLibrary,
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
