// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "os.h"

#include <sw/manager/version.h>

#include <primitives/command.h>
#include <primitives/templates.h>
#include <primitives/sw/settings.h>
#include <primitives/sw/cl.h>

#include <boost/algorithm/string.hpp>

#ifdef CPPAN_OS_WINDOWS_NO_CYGWIN
#include <windows.h>
#endif

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "os");

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
        c.arguments = { "uname", "-o" };
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

    if (os.Type == OSType::Android)
    {
        if (os.Arch == ArchType::arm)
        {
            if (os.SubArch == SubArchType::NoSubArch)
                os.SubArch = SubArchType::ARMSubArch_v7;
        }
    }

    if (os.Type == OSType::UnknownOS)
        throw SW_RUNTIME_ERROR("Unknown OS");

    return os;
}

const OS &getHostOS()
{
    static const auto os = detectOS();
    return os;
}

path getProgramFilesX86()
{
    auto e = getenv("programfiles(x86)");
    if (!e)
        throw SW_RUNTIME_ERROR("Cannot get 'programfiles(x86)' env. var.");
    return e;
}

static path getWindowsKitRoot()
{
    auto p = getProgramFilesX86() / "Windows Kits";
    if (fs::exists(p))
        return p;
    throw SW_RUNTIME_ERROR("No Windows Kits available");
}

static Strings listWindowsKits()
{
    Strings kits;
    auto kr = getWindowsKitRoot();
    for (auto &k : Strings{ getWin10KitDirName(), "8.1", "8.0", "7.1A", "7.0A", "6.0A" })
    {
        auto d = kr / k;
        if (fs::exists(d))
            kits.push_back(k);
    }
    return kits;
}

static path getLatestWindowsKit()
{
    auto allkits = listWindowsKits();
    if (allkits.empty())
        throw SW_RUNTIME_ERROR("No Windows Kits available");
    return allkits[0];
}

static path getWin10KitInspectionDir()
{
    auto kr = getWindowsKitRoot();
    auto dir = kr / getWin10KitDirName() / "Include";
    return dir;
}

static std::set<path> listWindows10Kits()
{
    std::set<path> kits;
    auto dir = getWin10KitInspectionDir();
    for (auto &i : fs::directory_iterator(dir))
    {
        if (fs::is_directory(i))
        {
            auto d = i.path().filename().u8string();
            Version v = d;
            if (v.isVersion())
                kits.insert(d);
        }
    }
    if (kits.empty())
        throw SW_RUNTIME_ERROR("No Windows 10 Kits available");
    return kits;
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

String OS::getLibraryPrefix() const
{
    switch (Type)
    {
    case OSType::Cygwin: // empty for cygwin or lib?
    case OSType::Windows:
        return "";
    default:
        return "lib";
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

String OS::getObjectFileExtension() const
{
    switch (Type)
    {
    case OSType::Windows:
        return ".obj";
    default:
        return ".o";
    }
}

bool OS::operator<(const OS &rhs) const
{
    return std::tie(Type, Arch, SubArch, Version) <
        std::tie(rhs.Type, rhs.Arch, rhs.SubArch, rhs.Version);
}

bool OS::operator==(const OS &rhs) const
{
    return std::tie(Type, Arch, SubArch, Version) ==
        std::tie(rhs.Type, rhs.Arch, rhs.SubArch, rhs.Version);
}

OsSdk::OsSdk(const OS &TargetOS)
{
    if (TargetOS.is(OSType::Windows))
    {
        if (Root.empty())
            Root = getWindowsKitRoot();
        if (Version.empty())
            Version = getLatestWindowsKit();
        if (BuildNumber.empty())
        {
            if (TargetOS.Version >= ::sw::Version(10) && Version == getWin10KitDirName())
            {
                // take current or the latest version!
                // sometimes current does not work:
                //  on appveyor we have win10.0.14393.0, but no sdk
                //  but we have the latest sdk there: win10.0.17763.0
                auto dir = getWin10KitInspectionDir();
                path cursdk = TargetOS.Version.toString(4);
                path curdir = dir / cursdk;
                // also check for some executable inside our dir
                if (fs::exists(curdir) &&
                    (fs::exists(getPath("bin") / cursdk / "x64" / "rc.exe") ||
                        fs::exists(getPath("bin") / cursdk / "x86" / "rc.exe")))
                    BuildNumber = curdir.filename();
                else
                    BuildNumber = *listWindows10Kits().rbegin();
            }
        }
    }
    else if (TargetOS.is(OSType::Macos) || TargetOS.is(OSType::IOS))
    {
        if (Root.empty())
        {
            String sdktype = "macosx";
            if (TargetOS.is(OSType::IOS))
                sdktype = "iphoneos";

            primitives::Command c;
            c.setProgram("xcrun");
            c.arguments.push_back("--sdk");
            c.arguments.push_back(sdktype);
            c.arguments.push_back("--show-sdk-path");
            error_code ec;
            c.execute(ec);
            if (ec)
            {
                LOG_DEBUG(logger, "cannot find " + sdktype + " sdk path using xcrun");
            }
            else
            {
                Root = boost::trim_copy(c.out.text);
            }
        }
    }
}

path OsSdk::getPath(const path &subdir) const
{
    if (Root.empty())
        throw SW_RUNTIME_ERROR("empty sdk root");
    //if (Version.empty())
    //throw SW_RUNTIME_ERROR("empty sdk version, root is: " + normalize_path(Root));
    if (subdir.empty())
        return Root / Version;
    return Root / Version / subdir / BuildNumber;
}

String OsSdk::getWindowsTargetPlatformVersion() const
{
    if (Version != getWin10KitDirName())
        return Version.u8string();
    return BuildNumber.u8string();
}

void OsSdk::setAndroidApiVersion(int v)
{
    Version = std::to_string(v);
}

/*bool OsSdk::operator<(const SDK &rhs) const
{
    return std::tie(Root, Version, BuildNumber) < std::tie(rhs.Root, rhs.Version, rhs.BuildNumber);
}

bool OsSdk::operator==(const SDK &rhs) const
{
    return std::tie(Root, Version, BuildNumber) == std::tie(rhs.Root, rhs.Version, rhs.BuildNumber);
}*/

String getWin10KitDirName()
{
    return "10";
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
