// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include "settings.h"

#include <sw/builder/os.h>

namespace sw
{

PackageSettings toPackageSettings(const OS &o)
{
    PackageSettings s;
    switch (o.Type)
    {
    case OSType::Windows:
        s["os"]["kernel"] = "com.Microsoft.Windows.NT";
        break;
    case OSType::Linux:
        s["os"]["kernel"] = "org.torvalds.linux";
        break;
    case OSType::Macos:
        s["os"]["kernel"] = "com.Apple.Macos";
        break;
    case OSType::Darwin:
        s["os"]["kernel"] = "com.Apple.Darwin";
        break;
    case OSType::Cygwin:
        s["os"]["kernel"] = "org.cygwin";
        break;
    case OSType::Mingw:
        s["os"]["kernel"] = "org.mingw";
        break;
    default:
        SW_UNIMPLEMENTED;
    }
    // do not specify, just takes max available
    //s["os"]["version"] = o.Version.toString();

    switch (o.Arch)
    {
    case ArchType::x86:
        s["os"]["arch"] = "x86";
        break;
    case ArchType::x86_64:
        s["os"]["arch"] = "x86_64";
        break;
    case ArchType::arm:
        s["os"]["arch"] = "arm";
        break;
    case ArchType::aarch64:
        s["os"]["arch"] = "aarch64";
        break;
    default:
        SW_UNIMPLEMENTED;
    }

    /*switch (o.SubArch)
    {
    case SubArchType:::
    s["os"]["subarch"] = "";
    break;
    default:
    SW_UNIMPLEMENTED;
    }*/

    switch (o.EnvType)
    {
    case EnvironmentType::GNUEABI:
        s["os"]["environment"] = "gnueabi";
        break;
    case EnvironmentType::GNUEABIHF:
        s["os"]["environment"] = "gnueabihf";
        break;
    }

    // we might not have sdk version installed
    //s["os"]["version"] = o.Version.toString();

    return s;
}

} // namespace sw
