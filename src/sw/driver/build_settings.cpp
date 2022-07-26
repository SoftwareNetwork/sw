// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include "build_settings.h"

#include <boost/algorithm/string.hpp>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "build_settings");

namespace sw
{

// move to OS?
static OS fromTargetSettings(const TargetSettings &ts)
{
    OS os;

#define IF_SETTING(s, var, value)   \
    else if (v == s)                \
        var = value

#define IF_SETTING_ANY_CASE(s, var, value)                                  \
    else if (boost::to_lower_copy(v.getValue()) == boost::to_lower_copy(s)) \
        var = value

#define IF_KEY(k)           \
    {                       \
        auto &v = ts[k];    \
        if (v) {

#define IF_END }}

    IF_KEY("os"]["kernel")
        if (0);
        IF_SETTING("com.Microsoft.Windows.NT", os.Type, OSType::Windows);
        IF_SETTING("org.torvalds.linux", os.Type, OSType::Linux);
        IF_SETTING("com.google.android", os.Type, OSType::Android);
        IF_SETTING("com.Apple.Macos", os.Type, OSType::Macos);
        IF_SETTING("com.Apple.Darwin", os.Type, OSType::Darwin);
        IF_SETTING("org.cygwin", os.Type, OSType::Cygwin);
        IF_SETTING("org.mingw", os.Type, OSType::Mingw);
        IF_SETTING("org.emscripten", os.Type, OSType::Wasm);
        else
            throw SW_RUNTIME_ERROR("Unknown os: " + v.getValue());
    IF_END
    if (os.Type == OSType::Android) {
        os.Type = OSType::Linux;
        os.Android = true;
    }

    IF_KEY("os"]["version")
        os.Version = v.getValue();
    IF_END

    IF_KEY("os"]["arch")
        if (0);
        IF_SETTING("x86", os.Arch, ArchType::x86);
        IF_SETTING("x86_64", os.Arch, ArchType::x86_64);
        IF_SETTING("arm", os.Arch, ArchType::arm);
        IF_SETTING("aarch64", os.Arch, ArchType::aarch64);
        IF_SETTING("wasm64", os.Arch, ArchType::wasm64);
        else
            throw SW_RUNTIME_ERROR("Unknown arch: " + v.getValue());
    IF_END

    IF_KEY("os"]["environment")
        if (0);
        IF_SETTING("gnueabi", os.EnvType, EnvironmentType::GNUEABI);
        IF_SETTING("gnueabihf", os.EnvType, EnvironmentType::GNUEABIHF);
        else
            throw SW_RUNTIME_ERROR("Unknown arch: " + v.getValue());
    IF_END

    return os;
}

BuildSettings::BuildSettings(const TargetSettings &ts)
{
    TargetOS = fromTargetSettings(ts);

    IF_KEY("native"]["library")
        if (0);
        IF_SETTING("static", Native.LibrariesType, LibraryType::Static);
        IF_SETTING("shared", Native.LibrariesType, LibraryType::Shared);
        else
            throw SW_RUNTIME_ERROR("Bad library type: " + v.getValue());
    IF_END

    IF_KEY("native"]["configuration")
        if (0);
        IF_SETTING_ANY_CASE("debug"s, Native.ConfigurationType, ConfigurationType::Debug);
        IF_SETTING_ANY_CASE("minimalsizerelease"s, Native.ConfigurationType, ConfigurationType::MinimalSizeRelease);
        IF_SETTING_ANY_CASE("release"s, Native.ConfigurationType, ConfigurationType::Release);
        IF_SETTING_ANY_CASE("releasewithdebuginformation"s, Native.ConfigurationType, ConfigurationType::ReleaseWithDebugInformation);
        else
            throw SW_RUNTIME_ERROR("Unknown configuration: " + v.getValue());
    IF_END

    IF_KEY("native"]["mt")
        Native.MT = v == "true";
    IF_END

#undef IF_SETTING
#undef IF_KEY
#undef IF_END
}

String BuildSettings::getTargetTriplet() const
{
    // See https://clang.llvm.org/docs/CrossCompilation.html

    String target;

    // arch
    target += toTripletString(TargetOS.Arch);
    if (TargetOS.Arch == ArchType::arm)
        target += toTripletString(TargetOS.SubArch);

    // vendor
    if (TargetOS.isApple())
        target += "-apple";
    else if (TargetOS.Type == OSType::Linux)
        target += "-linux";
    else
        target += "-unknown";

    // os
    if (TargetOS.EnvType != EnvironmentType::UnknownEnvironment)
    {
        switch (TargetOS.EnvType)
        {
        case EnvironmentType::GNUEABI:
            target += "-gnueabi";
            break;
        case EnvironmentType::GNUEABIHF:
            target += "-gnueabihf";
            break;
        }
    }
    else
    {
        target += "-" + toTripletString(TargetOS.Type);
        if (TargetOS.isApple() && TargetOS.Version)
            target += TargetOS.Version->toString(TargetOS.Version->getRealLevel());
        if (TargetOS.Type == OSType::Android)
            target += "-android";
        if (TargetOS.Arch == ArchType::arm)
            target += "eabi";
    }
    if (TargetOS.Type == OSType::Android)
    {
        SW_UNIMPLEMENTED;
        //target += Native.SDK.Version.string();
    }
    return target;
}

TargetSettings BuildSettings::getTargetSettings() const
{
    // DO NOT REMOVE any fields, because they do must be set in order to not fail relaxed comparison!
    //
    // TODO: also
    // FIXME: assign special settings value that will think value is different
    //
    // For example:
    //
    // 1. default case: shared lib
    // then: set s["native"]["library"].ForceInComparison(); or something like that
    //
    // 2. default case: mt = false
    // then: same as above
    //

    auto s = toTargetSettings(TargetOS);

    switch (Native.LibrariesType)
    {
    case LibraryType::Static:
        s["native"]["library"] = "static";
        break;
    case LibraryType::Shared:
        s["native"]["library"] = "shared";
        break;
    default:
        SW_UNIMPLEMENTED;
    }

    switch (Native.ConfigurationType)
    {
    case ConfigurationType::Debug:
        s["native"]["configuration"] = "Debug";
        break;
    case ConfigurationType::MinimalSizeRelease:
        s["native"]["configuration"] = "MinimalSizeRelease";
        break;
    case ConfigurationType::Release:
        s["native"]["configuration"] = "Release";
        break;
    case ConfigurationType::ReleaseWithDebugInformation:
        s["native"]["configuration"] = "ReleaseWithDebugInformation";
        break;
    default:
        SW_UNIMPLEMENTED;
    }

    if (TargetOS.is(OSType::Windows))
        s["native"]["mt"] = Native.MT ? "true" : "false";

    // debug, release, ...

    return s;
}

}
