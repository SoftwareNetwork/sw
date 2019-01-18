// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "enums.h"

#include <configuration.h>

#include <boost/assign.hpp>
#include <primitives/filesystem.h>

#include <atomic>
#include <optional>

namespace sw
{

enum class CompilerType
{
    UnspecifiedCompiler,

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

struct InheritanceScope
{
    enum
    {
        Package = 1 << 0,
        Project = 1 << 1, // consists of packages
        Other   = 1 << 2, // consists of projects and packages

        Private = Package,
        Group = Project,
        World = Other,
    };
};

enum class InheritanceType
{
    // 8 types
    // - 000 type (invalid)
    // = 7 types

    // 001 - usual private options
    Private = InheritanceScope::Package,

    // 011 - private and project
    Protected = InheritanceScope::Package | InheritanceScope::Project,

    // 111 - everyone
    Public = InheritanceScope::Package | InheritanceScope::Project | InheritanceScope::World,

    // 110 - project and others
    Interface = InheritanceScope::Project | InheritanceScope::World,

    // rarely used

    // 100 - only others
    // TODO: set new name?
    ProjectInterface = InheritanceScope::World,
    // or ProtectedInterface?

    // 010 - Project?
    // TODO: set new name
    ProjectOnly = InheritanceScope::Project,

    // 101 - package and others
    // TODO: set new name
    NotProject = InheritanceScope::Package | InheritanceScope::World,

    // alternative names

    Default = Private,
    Min = Private,
    Max = Public + 1,
};

//InheritanceType inherit(InheritanceType InType, InheritanceType ChildObject, bool SameProject = false);

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
    C90 = C89,
    C95,
    C98,
    C11,
    C17,
    C18 = C17,

    CLatest = C18,
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
};

struct SW_DRIVER_CPP_API GroupSettings
{
    InheritanceType Inheritance = InheritanceType::Private;
    ConfigurationType Configuration = ConfigurationType::Release;
    bool has_same_parent = false;
    bool merge_to_self = true;
    bool dependencies_only = false;
};

template <class T>
struct SW_DRIVER_CPP_API IterableOptions
{
    template <class F, class ... Args>
    auto iterate(F &&f, const GroupSettings &s = GroupSettings())
    {
        std::forward<F>(f)(*(T *)this, s);
    }

    template <class F, class ... Args>
    auto iterate(F &&f, const GroupSettings &s = GroupSettings()) const
    {
        std::forward<F>(f)(*(T *)this, s);
    }
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
    //path SourceDirBase; // "root" real source dir
    path SourceDir; // "current" source dir
    path BinaryDir;
    path BinaryPrivateDir;

    //void restoreSourceDir() { SourceDir = SourceDirBase; }
};

String toString(CompilerType Type);
String toString(LinkerType Type);
String toString(InheritanceType Type);
String toString(LibraryType Type);
String toString(ConfigurationType Type);

//CompilerType compilerTypeFromString(const String &s);
//ConfigurationType configurationTypeFromString(const String &s);

CompilerType compilerTypeFromStringCaseI(const String &s);
ConfigurationType configurationTypeFromStringCaseI(const String &s);

struct Configuration : ConfigurationBase
{
    //
    ConfigurationType Type = ConfigurationType::Release;

    bool DebugInfo = false;
    bool Optimized = false;
    bool MinimalSize = false;
    bool Analyze = false;
    bool Sanitize = false;
    bool Profile = false;
};

}
