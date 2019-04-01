// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <os.h>

#include <primitives/command.h>
#include <primitives/templates.h>
#include <primitives/sw/settings.h>
#include <primitives/sw/cl.h>

#include <boost/algorithm/string.hpp>

#ifdef CPPAN_OS_WINDOWS_NO_CYGWIN
#include <windows.h>
#endif

static cl::opt<bool> allow_cygwin_hosts("host-cygwin", cl::desc("When on cygwin, allow it as host"));

namespace sw
{

namespace detail
{

bool isHostCygwin()
{
    static auto cyg = []()
    {
        primitives::Command c;
        c.args = { "uname", "-o" };
        error_code ec;
        c.execute(ec);
        if (!ec)
        {
            boost::trim(c.out.text);
            if (boost::iequals(c.out.text, "cygwin"))
                return true;
        }
        return false;
    }();
    return cyg;
}

} // namespace detail

OS detectOS()
{
    OS os;

#if defined(CPPAN_OS_WINDOWS)
    os.Type = OSType::Windows;
#endif

#ifdef CPPAN_OS_CYGWIN
    os.Type = OSType::Cygwin;
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

    if (allow_cygwin_hosts)
    {
        if (detail::isHostCygwin())
            os.Type = OSType::Cygwin;
    }
#endif

#if defined(CPPAN_OS_LINUX)
    os.Type = OSType::Linux;
    os.Arch = ArchType::x86_64;
#endif

#if defined(CPPAN_OS_APPLE)
    os.Type = OSType::Macos;
    os.Arch = ArchType::x86_64;
#endif

    // TODO: uname -a on *nix
    //#include <sys/utsname.h>
    //int uname(struct utsname *buf);

    if (os.Type == OSType::UnknownOS)
        throw SW_RUNTIME_ERROR("Unknown OS");

    return os;
}

const OS &getHostOS()
{
    static const auto os = detectOS();
    return os;
}

bool OS::canRunTargetExecutables(const OS &TargetOS) const
{
    auto cannotRunTargetExecutables = [this, &TargetOS]()
    {
        if (Type != TargetOS.Type)
            return true;
        if (Arch != TargetOS.Arch)
        {
            if (Type == OSType::Windows &&
                Arch == ArchType::x86_64 && TargetOS.Arch == ArchType::x86
                )
                ; // win64 can run win32, but not vice versa
            else
                return true;
        }
        return false;
    };
    return !cannotRunTargetExecutables();
}

ShellType OS::getShellType() const
{
    switch (Type)
    {
    case OSType::Windows:
        return ShellType::Batch;
    default:
        return ShellType::Shell;
    }
}

String OS::getShellExtension() const
{
    switch (getShellType())
    {
    case ShellType::Batch:
        return ".bat";
    default:
        return ".sh";
    }
}

String OS::getExecutableExtension() const
{
    switch (Type)
    {
    case OSType::Cygwin:
    case OSType::Windows:
        return ".exe";
    default:
        return "";
    }
}

String OS::getStaticLibraryExtension() const
{
    switch (Type)
    {
    case OSType::Windows:
        return ".lib";
    default:
        return ".a";
    }
}

String OS::getSharedLibraryExtension() const
{
    switch (Type)
    {
    case OSType::Cygwin:
    case OSType::Windows:
        return ".dll";
    case OSType::Macos:
    case OSType::IOS:
        return ".dylib";
    default:
        return ".so";
    }
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
        CASE(Macos);
        CASE(Cygwin);
        CASE(Android);
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
        CASE(arm);
        CASE(aarch64);
    default:
        throw std::logic_error("TODO: implement target arch");
    }
#undef CASE
#undef ENUM
}

String toStringWindows(ArchType e)
{
    switch (e)
    {
    case ArchType::x86_64:
        return "x64";
    case ArchType::x86:
        return "x86";
    case ArchType::arm:
        return "arm";
    case ArchType::aarch64:
        return "arm64";
    default:
        throw SW_RUNTIME_ERROR("Unknown Windows arch");
    }
}

String toString(SubArchType e)
{
    switch (e)
    {
    case SubArchType::NoSubArch:
        return "";
    case SubArchType::ARMSubArch_v7:
        return "v7a";
    default:
        throw SW_RUNTIME_ERROR("not implemented");
    }
}

String toTripletString(OSType e)
{
    return boost::to_lower_copy(toString(e));
}

String toTripletString(ArchType e)
{
    return boost::to_lower_copy(toString(e));
}

String toTripletString(SubArchType e)
{
    return boost::to_lower_copy(toString(e));
}

String toString(EnvironmentType e)
{
    throw SW_RUNTIME_ERROR("not implemented");
}

String toString(ObjectFormatType e)
{
    throw SW_RUNTIME_ERROR("not implemented");
}

OSType OSTypeFromStringCaseI(const String &target_os)
{
    if (boost::iequals(target_os, "linux"))
        return OSType::Linux;
    else if (boost::iequals(target_os, "macos"))
        return OSType::Macos;
    else if (boost::iequals(target_os, "windows") ||
        boost::iequals(target_os, "win"))
        return OSType::Windows;
    else if (boost::iequals(target_os, "cygwin"))
        return OSType::Cygwin;
    else if (!target_os.empty())
        throw SW_RUNTIME_ERROR("Unknown target_os: " + target_os);
    return OSType::UnknownOS;
}

ArchType archTypeFromStringCaseI(const String &platform)
{
    if (boost::iequals(platform, "Win32") ||
        boost::iequals(platform, "x86"))
        return ArchType::x86;
    else if (
        boost::iequals(platform, "Win64") ||
        boost::iequals(platform, "x64") ||
        boost::iequals(platform, "x86_64") ||
        boost::iequals(platform, "x64_86"))
        return ArchType::x86_64;
    else if (
        boost::iequals(platform, "arm32") ||
        boost::iequals(platform, "arm"))
        return ArchType::arm;
    else if (boost::iequals(platform, "arm64") ||
        boost::iequals(platform, "aarch64"))
        return ArchType::aarch64; // ?
    else if (!platform.empty())
        throw SW_RUNTIME_ERROR("Unknown platform: " + platform);
    return ArchType::UnknownArch;
}

}
