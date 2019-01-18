// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "types.h"

#include <solution.h>
#include <target.h>

#include <boost/algorithm/string.hpp>

namespace sw
{

/*InheritanceType inherit(InheritanceType InType, InheritanceType ChildObject, bool SameProject)
{
    if (ChildObject == InheritanceType::Private)
        throw std::logic_error("Invalid case. You must filter it yourself.");

    std::optional<int>

    switch (InType)
    {
    case InheritanceType::Private:
        return InheritanceType::Private;
    case InheritanceType::Protected:
        return InheritanceType::Protected;
    case InheritanceType::Public:
    case InheritanceType::Interface:
        break;
    }
}*/

bool Assigner::canProceed(TargetOptions &r) const
{
    if (allow)
        return allow.value();
    //if (r.target->solution->Settings.Native.AssignAll)
        //return true;
    if (LT == LibraryType::Unspecified)
        return true;
    auto t = r.target->getType();
    if (t == TargetType::NativeLibrary && r.target->solution->Settings.Native.LibrariesType != LT)
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
}

String toString(CompilerType Type)
{
    switch (Type)
    {
#define CASE(x)           \
    case CompilerType::x: \
        return #x

        CASE(Clang);
        CASE(ClangCl);
        CASE(GNU);
        CASE(MSVC);
        CASE(Other);
    default:
        throw std::logic_error("todo: implement compiler type");
    }
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
}

CompilerType compilerTypeFromStringCaseI(const String &compiler)
{
    if (boost::iequals(compiler, "clang"))
        return CompilerType::Clang;
    else if (boost::iequals(compiler, "clangcl") || boost::iequals(compiler, "clang-cl"))
        return CompilerType::ClangCl;
    else if (boost::iequals(compiler, "gnu"))
        return CompilerType::GNU;
    else if (boost::iequals(compiler, "msvc"))
        return CompilerType::MSVC;
    else if (!compiler.empty())
        throw SW_RUNTIME_ERROR("Unknown compiler: " + compiler);
    return CompilerType::UnspecifiedCompiler;
}

ConfigurationType configurationTypeFromStringCaseI(const String &configuration)
{
    if (boost::iequals(configuration, "Debug"))
        return ConfigurationType::Debug;
    else if (boost::iequals(configuration, "Release"))
        return ConfigurationType::Release;
    else if (boost::iequals(configuration, "MinSizeRel") ||
        boost::iequals(configuration, "MinimalSizeRelease"))
        return ConfigurationType::MinimalSizeRelease;
    else if (boost::iequals(configuration, "RelWithDebInfo") ||
        boost::iequals(configuration, "ReleaseWithDebugInformation") ||
        boost::iequals(configuration, "ReleaseWithDebInfo"))
        return ConfigurationType::ReleaseWithDebugInformation;
    else if (!configuration.empty())
        throw SW_RUNTIME_ERROR("Unknown configuration: " + configuration);
    return ConfigurationType::Unspecified;
}

} // namespace sw
