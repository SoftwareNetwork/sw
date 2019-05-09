// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "types.h"

#include "solution.h"
#include "target/base.h"

#include <boost/algorithm/string.hpp>

namespace sw
{

bool isClangFamily(CompilerType t)
{
    return t == CompilerType::AppleClang ||
           t == CompilerType::Clang ||
           t == CompilerType::ClangCl;
}

bool Assigner::canProceed(TargetOptions &r) const
{
    if (allow)
        return allow.value();
    //if (r.target->solution->Settings.Native.AssignAll)
        //return true;
    if (LT == LibraryType::Unspecified)
        return true;
    auto t = r.target->getType();
    if (t == TargetType::NativeLibrary && r.target->getSettings().Native.LibrariesType != LT)
        return false;
    // executables are always shared objects
    if (t == TargetType::NativeExecutable && LT != LibraryType::Shared)
        return false;
    if (t == TargetType::NativeStaticLibrary && LT != LibraryType::Static)
        return false;
    if (t == TargetType::NativeSharedLibrary && LT != LibraryType::Shared)
        return false;
    return true;
}

String toString(ConfigurationType Type)
{
    switch (Type)
    {
#define CASE(x)                \
    case ConfigurationType::x: \
        return #x

        CASE(Debug);
        CASE(MinimalSizeRelease);
        CASE(Release);
        CASE(ReleaseWithDebugInformation);

    default:
        throw std::logic_error("todo: implement config type");
    }
#undef CASE
}

String toString(CompilerType Type)
{
    switch (Type)
    {
#define CASE(x)           \
    case CompilerType::x: \
        return #x

        CASE(AppleClang);
        CASE(Clang);
        CASE(ClangCl);
        CASE(GNU);
        CASE(MSVC);
        CASE(Other);

    case CompilerType::UnspecifiedCompiler:
        throw std::logic_error("Compiler type was not set");
    default:
        throw std::logic_error("TODO: implement compiler type");
    }
#undef CASE
}

String toString(LinkerType Type)
{
    switch (Type)
    {
#define CASE(x)         \
    case LinkerType::x: \
        return #x

        CASE(LLD);
        CASE(MSVC);

    default:
        throw std::logic_error("todo: implement linker type");
    }
#undef CASE
}

String toString(InheritanceType Type)
{
    switch (Type)
    {
#define CASE(x)              \
    case InheritanceType::x: \
        return #x

        CASE(Private);
        CASE(Protected);
        CASE(Public);
        CASE(Interface);

    default:
        throw std::logic_error("todo: implement inheritance type");
    }
#undef CASE
}

String toString(LibraryType Type)
{
    switch (Type)
    {
#define CASE(x)          \
    case LibraryType::x: \
        return #x

        CASE(Static);
        CASE(Shared);

    default:
        throw std::logic_error("todo: implement inheritance type");
    }
#undef CASE
}

CompilerType compilerTypeFromStringCaseI(const String &compiler)
{
    if (0);
    // exact
    else if (boost::iequals(compiler, "clang"))
        return CompilerType::Clang;
    else if (boost::iequals(compiler, "clangcl") || boost::iequals(compiler, "clang-cl"))
        return CompilerType::ClangCl;
    // starts with
    else if (boost::istarts_with(compiler, "appleclang") || boost::iequals(compiler, "apple-clang"))
        return CompilerType::AppleClang;
    else if (boost::istarts_with(compiler, "gnu") || boost::iequals(compiler, "gcc") || boost::iequals(compiler, "g++"))
        return CompilerType::GNU;
    else if (boost::istarts_with(compiler, "msvc") || boost::istarts_with(compiler, "vs"))
        return CompilerType::MSVC;
    //
    else if (!compiler.empty())
        throw SW_RUNTIME_ERROR("Unknown compiler: " + compiler);
    return CompilerType::UnspecifiedCompiler;
}

ConfigurationType configurationTypeFromStringCaseI(const String &configuration)
{
    if (boost::iequals(configuration, "Debug") ||
        boost::iequals(configuration, "d"))
        return ConfigurationType::Debug;
    else if (boost::iequals(configuration, "Release") ||
        boost::iequals(configuration, "r"))
        return ConfigurationType::Release;
    else if (boost::iequals(configuration, "MinSizeRel") ||
        boost::iequals(configuration, "msr") ||
        boost::iequals(configuration, "MinimalSizeRelease"))
        return ConfigurationType::MinimalSizeRelease;
    else if (boost::iequals(configuration, "RelWithDebInfo") ||
        boost::iequals(configuration, "ReleaseWithDebugInformation") ||
        boost::iequals(configuration, "rwdi") ||
        boost::iequals(configuration, "ReleaseWithDebInfo"))
        return ConfigurationType::ReleaseWithDebugInformation;
    else if (!configuration.empty())
        throw SW_RUNTIME_ERROR("Unknown configuration: " + configuration);
    return ConfigurationType::Unspecified;
}

} // namespace sw
