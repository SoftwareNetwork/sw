// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "build_settings.h"

#include <boost/algorithm/string.hpp>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "build_settings");

namespace sw
{

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

String getWin10KitDirName()
{
    return "10";
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

void BuildSettings::init()
{
    if (TargetOS.is(OSType::Windows))
    {
        if (Native.SDK.Root.empty())
            Native.SDK.Root = getWindowsKitRoot();
        if (Native.SDK.Version.empty())
            Native.SDK.Version = getLatestWindowsKit();
        if (Native.SDK.BuildNumber.empty())
        {
            if (TargetOS.Version >= Version(10) && Native.SDK.Version == getWin10KitDirName())
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
                    (fs::exists(Native.SDK.getPath("bin") / cursdk / "x64" / "rc.exe") ||
                        fs::exists(Native.SDK.getPath("bin") / cursdk / "x86" / "rc.exe")))
                    Native.SDK.BuildNumber = curdir.filename();
                else
                    Native.SDK.BuildNumber = *listWindows10Kits().rbegin();
            }
        }
    }
    else if (TargetOS.is(OSType::Macos) || TargetOS.is(OSType::IOS))
    {
        if (Native.SDK.Root.empty())
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
                Native.SDK.Root = boost::trim_copy(c.out.text);
            }
        }
    }
    else if (TargetOS.Type == OSType::Android)
    {
        if (TargetOS.Arch == ArchType::arm)
        {
            if (TargetOS.SubArch == SubArchType::NoSubArch)
                TargetOS.SubArch = SubArchType::ARMSubArch_v7;
        }
    }
}

// move to OS?
static OS fromTargetSettings(const TargetSettings &ts)
{
    OS os;

#define IF_SETTING(s, var, value) \
    else if (i->second == s)      \
        var = value

#define IF_KEY(s)            \
    {{                       \
        auto i = ts.find(s); \
        if (i != ts.end())
#define IF_END }}

    IF_KEY("os.kernel")
        if (0);
        IF_SETTING("com.Microsoft.Windows.NT", os.Type, OSType::Windows);
        else
            SW_UNIMPLEMENTED;
    IF_END

    IF_KEY("os.version")
        os.Version = i->second;
    IF_END

    IF_KEY("os.arch")
        if (0);
        IF_SETTING("x86", os.Arch, ArchType::x86);
        IF_SETTING("x86_64", os.Arch, ArchType::x86_64);
        IF_SETTING("arm", os.Arch, ArchType::arm);
        IF_SETTING("aarch64", os.Arch, ArchType::aarch64);
        else
            SW_UNIMPLEMENTED;
    IF_END

    return os;
}

static TargetSettings toTargetSettings(const OS &o)
{
    TargetSettings s;
    switch (o.Type)
    {
    case OSType::Windows:
        s["os.kernel"] = "com.Microsoft.Windows.NT";
        break;
    default:
        SW_UNIMPLEMENTED;
    }
    s["os.version"] = o.Version.toString();

    switch (o.Arch)
    {
    case ArchType::x86:
        s["os.arch"] = "x86";
        break;
    case ArchType::x86_64:
        s["os.arch"] = "x86_64";
        break;
    case ArchType::arm:
        s["os.arch"] = "arm";
        break;
    case ArchType::aarch64:
        s["os.arch"] = "aarch64";
        break;
    default:
        SW_UNIMPLEMENTED;
    }

    /*switch (o.SubArch)
    {
    case SubArchType:::
    s["os.subarch"] = "";
    break;
    default:
    SW_UNIMPLEMENTED;
    }*/

    // no version at the moment
    // it is not clear if it's needed

    return s;
}

BuildSettings::BuildSettings(const TargetSettings &ts)
{
    TargetOS = fromTargetSettings(ts);

    IF_KEY("compiler.c")
        if (0);
        IF_SETTING("msvc-version", Native.CompilerType1, CompilerType::MSVC);
        else
            SW_UNIMPLEMENTED;
    IF_END

    IF_KEY("library")
        if (0);
        IF_SETTING("static", Native.LibrariesType, LibraryType::Static);
        IF_SETTING("shared", Native.LibrariesType, LibraryType::Shared);
        else
            SW_UNIMPLEMENTED;
    IF_END

    IF_KEY("configuration")
        if (0);
        IF_SETTING("Debug", Native.ConfigurationType, ConfigurationType::Debug);
        IF_SETTING("MinimalSizeRelease", Native.ConfigurationType, ConfigurationType::Debug);
        IF_SETTING("Release", Native.ConfigurationType, ConfigurationType::Release);
        IF_SETTING("ReleaseWithDebugInformation", Native.ConfigurationType, ConfigurationType::ReleaseWithDebugInformation);
        else
            SW_UNIMPLEMENTED;
    IF_END

    IF_KEY("mt")
        Native.MT = i->second == "true";
    IF_END

#undef IF_SETTING
#undef IF_KEY
#undef IF_END

    init();
}

String BuildSettings::getConfig() const
{
    // TODO: add get real config, lengthy and with all info

    String c;

    addConfigElement(c, toString(TargetOS.Type));
    if (TargetOS.Type == OSType::Android)
        addConfigElement(c, Native.SDK.Version.string());
    addConfigElement(c, toString(TargetOS.Arch));
    if (TargetOS.Arch == ArchType::arm || TargetOS.Arch == ArchType::aarch64)
        addConfigElement(c, toString(TargetOS.SubArch)); // concat with previous?

    addConfigElement(c, toString(Native.LibrariesType));
    if (TargetOS.Type == OSType::Windows && Native.MT)
        addConfigElement(c, "mt");
    addConfigElement(c, toString(Native.ConfigurationType));

    return c;
}

String BuildSettings::getTargetTriplet() const
{
    // See https://clang.llvm.org/docs/CrossCompilation.html

    String target;
    target += toTripletString(TargetOS.Arch);
    if (TargetOS.Arch == ArchType::arm)
        target += toTripletString(TargetOS.SubArch);
    target += "-unknown"; // vendor
    target += "-" + toTripletString(TargetOS.Type);
    if (TargetOS.Type == OSType::Android)
        target += "-android";
    if (TargetOS.Arch == ArchType::arm)
        target += "eabi";
    if (TargetOS.Type == OSType::Android)
        target += Native.SDK.Version.string();
    return target;
}

bool BuildSettings::operator<(const BuildSettings &rhs) const
{
    return std::tie(TargetOS, Native) < std::tie(rhs.TargetOS, rhs.Native);
}

bool BuildSettings::operator==(const BuildSettings &rhs) const
{
    return std::tie(TargetOS, Native) == std::tie(rhs.TargetOS, rhs.Native);
}

TargetSettings BuildSettings::getTargetSettings() const
{
    TargetSettings s;
    s.merge(toTargetSettings(TargetOS));

    switch (Native.CompilerType1)
    {
    case CompilerType::UnspecifiedCompiler:
        break;
    case CompilerType::MSVC:
        s["compiler.c"] = "msvc-version";
        break;
    default:
        SW_UNIMPLEMENTED;
    }

    switch (Native.LibrariesType)
    {
    case LibraryType::Static:
        s["library"] = "static";
        break;
    case LibraryType::Shared:
        s["library"] = "shared";
        break;
    default:
        SW_UNIMPLEMENTED;
    }

    switch (Native.ConfigurationType)
    {
    case ConfigurationType::Debug:
        s["configuration"] = "Debug";
        break;
    case ConfigurationType::MinimalSizeRelease:
        s["configuration"] = "MinimalSizeRelease";
        break;
    case ConfigurationType::Release:
        s["configuration"] = "Release";
        break;
    case ConfigurationType::ReleaseWithDebugInformation:
        s["configuration"] = "ReleaseWithDebugInformation";
        break;
    default:
        SW_UNIMPLEMENTED;
    }

    if (TargetOS.is(OSType::Windows))
        s["mt"] = Native.MT ? "true" : "false";

    // debug, release, ...

    return s;
}

}
