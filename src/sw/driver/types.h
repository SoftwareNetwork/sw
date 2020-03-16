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

#include <sw/builder/configuration.h>
#include <sw/manager/enums.h>

#include <boost/assign.hpp>
#include <primitives/filesystem.h>

#include <atomic>
#include <optional>

namespace sw
{

enum class CompilerType
{
    UnspecifiedCompiler,

    AppleClang,
    Clang,
    ClangCl,
    GNU,
    Intel, // ICC
    MSVC,
    // more

    Other, // custom compilers
    // zapcc - clang?
    // cuda

    // aliases
    GCC = GNU,
};

// any clang (clang, clangcl, appleclang)
SW_DRIVER_CPP_API
bool isClangFamily(CompilerType);

// msvc, clangcl
SW_DRIVER_CPP_API
bool isMsvcFamily(CompilerType);

enum class LinkerType
{
    UnspecifiedLinker,

    Gold,
    GNU,
    LLD,
    MSVC,
    // more

    LD = GNU,
};

enum class LibraryType
{
    Unspecified,

    Static,
    Shared,

    Default = Shared,
};

using BuildLibrariesAs = LibraryType;

enum class ConfigurationType : int32_t
{
    Unspecified,

    // main
    Debug,
    MinimalSizeRelease,
    Release,
    ReleaseWithDebugInformation,

    // other common
    Analyze,
    Benchmark,
    Coverage,
    Documentation,
    Profile,
    Sanitize,
    Test,
    UnitTest,
    Valgrind,

    MaxType,

    Default = Release,
};

enum class CLanguageStandard
{
    Unspecified,

    ANSI,
    C89 = ANSI,
    C90 = C89, // ansi is different from c90?
    C95,
    C99,
    C11,
    C17,
    C18 = C17,
    C2x,

    CLatest = C2x,

    // for quick standards
    cansi = ANSI,
    c89 = C89,
    c90 = C90,
    c95 = C95,
    c99 = C99,
    c11 = C11,
    c17 = C17,
    c18 = C18,
    c2x = C2x,
    clatest = CLatest,
};

enum class CPPLanguageStandard
{
    Unspecified,

    CPP98,
    CPP03 = CPP98,
    CPP11,
    CPP14,
    CPP17,
    CPP20,

    CPP0x = CPP11,
    CPP1y = CPP14,
    CPP1z = CPP17,
    CPP2a = CPP20,

    CPPLatest = CPP2a,

    // for quick standards
    cpp98 = CPP98,
    cpp03 = CPP03,
    cpp11 = CPP11,
    cpp14 = CPP14,
    cpp17 = CPP17,
    cpp20 = CPP20,

    cpp0x = CPP0x,
    cpp1y = CPP1y,
    cpp1z = CPP1z,
    cpp2a = CPP2a,

    cpplatest = CPPLatest,
};

template <class F>
struct stream_list_inserter : boost::assign::list_inserter<F>
{
    using boost::assign::list_inserter<F>::list_inserter;

    template <class T>
    stream_list_inserter& operator<<(const T& r)
    {
        this->operator,(r);
        return *this;
    }

    template <class T>
    stream_list_inserter& operator>>(const T& r)
    {
        this->operator,(r);
        return *this;
    }
};

template <class F>
inline stream_list_inserter<F> make_stream_list_inserter(F &&fun)
{
    return stream_list_inserter<F>(std::forward<F>(fun));
}

// native namespace?

// static/shared configs

static struct tag_static_t {} Static;
static struct tag_shared_t {} Shared;

// enum ConfigurationType

// and explicit tags for the most common configurations
static struct tag_debug_t {} Debug;
static struct tag_minimal_size_release_t {} MinimalSizeRelease;
static struct tag_release_t {} Release;
static struct tag_release_with_debug_information_t {} ReleaseWithDebugInformation;
// more? docs etc.

struct SW_DRIVER_CPP_API Assigner
{
    std::optional<bool> allow;
    LibraryType LT = LibraryType::Unspecified;

    bool canProceed(struct TargetOptions &r) const;

    void operator()(const sw::tag_static_t &) { LT = LibraryType::Static; }
    void operator()(const sw::tag_shared_t &) { LT = LibraryType::Shared; }
    void operator()(bool allow) { this->allow = allow; }
};

struct SW_DRIVER_CPP_API ProjectDirectories
{
    path SourceDirBase; // "root" real source dir
    path SourceDir; // "current" source dir
    path BinaryDir;
    path BinaryPrivateDir;

    void setSourceDirectory(const path &d);
};

String toString(CompilerType Type);
String toString(LinkerType Type);
String toString(LibraryType Type);
String toString(ConfigurationType Type);

}
