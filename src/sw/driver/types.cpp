// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include "types.h"

#include "build.h"
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

bool isMsvcFamily(CompilerType t)
{
    return
        t == CompilerType::MSVC ||
        t == CompilerType::ClangCl
        ;
}

bool Assigner::canProceed(TargetOptions &r) const
{
    if (allow)
        return allow.value();
    //if (r.target->solution->Settings.Native.AssignAll)
        //return true;
    if (LT == LibraryType::Unspecified)
        return true;
    auto t = r.getTarget().getType();
    if (t == TargetType::NativeLibrary && r.getTarget().getBuildSettings().Native.LibrariesType != LT)
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

void ProjectDirectories::setSourceDirectory(const path &d)
{
    if (d.empty())
        return;
    if (d.is_absolute())
    {
        SourceDirBase = d;
        SourceDir = d;
    }
    else
    {
        SourceDirBase /= d;
        SourceDir /= d;
    }
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

    case CompilerType::Unspecified:
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

} // namespace sw
