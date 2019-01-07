// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <os.h>

#include <primitives/templates.h>

#ifdef CPPAN_OS_WINDOWS_NO_CYGWIN
#include <windows.h>
#endif

namespace sw
{

OS detectOS()
{
    OS os;

#if defined(CPPAN_OS_WINDOWS)
    os.Type = OSType::Windows;
#endif

#ifdef CPPAN_OS_WINDOWS_NO_CYGWIN
    // TODO: set correct manifest
    /* Windows version number data.  */
    OSVERSIONINFOEXW osviex = { 0 };
    osviex.dwOSVersionInfoSize = sizeof(osviex);
    GetVersionExW((OSVERSIONINFOW*)&osviex);

    os.Version = { osviex.dwMajorVersion, osviex.dwMinorVersion, osviex.dwBuildNumber };

    auto a1 = getenv("PROCESSOR_ARCHITECTURE");
    auto a2 = getenv("PROCESSOR_ARCHITEW6432");
    auto check_env_var = [&os](auto a)
    {
        if (a && strcmp(a, "AMD64") == 0)
            os.Arch = ArchType::x86_64;
        if (a && strcmp(a, "x86") == 0)
            os.Arch = ArchType::x86;
    };
    check_env_var(a1);
    check_env_var(a2);
#endif

    // TODO: uname -a
#ifdef CPPAN_OS_CYGWIN
    os.type = OsType::Cygwin;
#endif

#if defined(CPPAN_OS_LINUX)
    os.Type = OSType::Linux;
    os.Arch = ArchType::x86_64;
#endif

    // TODO: uname -a on *nix
    //#include <sys/utsname.h>
    //int uname(struct utsname *buf);

    if (os.Type == OSType::UnknownOS)
        throw SW_RUNTIME_EXCEPTION("Unknown OS");

    return os;
}

String toString(OSType e)
{
#define ENUM OSType
#define CASE(x)   \
    case ENUM::x: \
        return #x

    switch (e)
    {
        CASE(Windows);
        CASE(Linux);
    default:
        throw std::logic_error("TODO: implement target os");
    }
#undef CASE
#undef ENUM
}

String toString(ArchType e)
{
#define ENUM ArchType
#define CASE(x)   \
    case ENUM::x: \
        return #x

    switch (e)
    {
        CASE(x86);
        CASE(x86_64);
    default:
        throw std::logic_error("TODO: implement target arch");
    }
#undef CASE
#undef ENUM
}

String toString(SubArchType e)
{
    return "";
}

String toString(EnvironmentType e)
{
    return "";
}

String toString(ObjectFormatType e)
{
    return "";
}

}
