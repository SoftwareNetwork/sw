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

#ifdef _WIN32
static Version GetWindowsVersion(void)
{
    typedef void (WINAPI * RtlGetVersion_FUNC) (OSVERSIONINFOEXW *);

    HMODULE hMod;
    RtlGetVersion_FUNC func;
    OSVERSIONINFOEXW osw = { 0 };

    hMod = LoadLibrary(TEXT("ntdll.dll"));
    if (!hMod)
        throw SW_RUNTIME_ERROR("Cannot load ntdll.dll");

    func = (RtlGetVersion_FUNC)GetProcAddress(hMod, "RtlGetVersion");
    if (!func)
    {
        FreeLibrary(hMod);
        throw SW_RUNTIME_ERROR("Cannot find RtlGetVersion");
    }
    osw.dwOSVersionInfoSize = sizeof(osw);
    func(&osw);
    FreeLibrary(hMod);
    return { osw.dwMajorVersion, osw.dwMinorVersion, osw.dwBuildNumber };
}
#endif

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
    os.Version = detail::GetWindowsVersion();

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

bool OS::canRunTargetExecutables(const OS &TargetOS) const
{
    if (Type != TargetOS.Type)
        return false;

    if (Arch != TargetOS.Arch)
    {
        // win64 can run win32, but not vice versa
        if (Type == OSType::Windows &&
            Arch == ArchType::x86_64 && TargetOS.Arch == ArchType::x86
            )
        {
            return true;
        }

        return false;
    }

    return true;
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
    case OSType::Darwin:
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

bool OS::isApple() const
{
    // macos/ios/tvos/watchos etc.
    return 0
        || Type == OSType::Darwin
        || Type == OSType::Macos
        || Type == OSType::IOS
        ;
}

/*struct SW_BUILDER_API OsSdk
{
    // root to sdks
    //  example: c:\\Program Files (x86)\\Windows Kits
    path Root;

    // sdk dir in root
    // win: 7.0 7.0A, 7.1, 7.1A, 8, 8.1, 10 ...
    // osx: 10.12, 10.13, 10.14 ...
    // android: 1, 2, 3, ..., 28
    path Version; // make string?

                  // windows10:
                  // 10.0.10240.0, 10.0.17763.0 ...
    path BuildNumber;

    OsSdk() = default;
    OsSdk(const OS &);
    OsSdk(const OsSdk &) = default;
    OsSdk &operator=(const OsSdk &) = default;

    path getPath(const path &subdir = {}) const;
    String getWindowsTargetPlatformVersion() const;
    void setAndroidApiVersion(int v);

    //bool operator<(const SDK &) const;
    //bool operator==(const SDK &) const;
};

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
}*/

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
        CASE(Darwin);
        CASE(IOS);
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

}
