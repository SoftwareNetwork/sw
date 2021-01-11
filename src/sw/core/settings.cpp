// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include "settings.h"

#include <sw/builder/os.h>
#include <sw/support/settings.h>

namespace sw
{

PackageSettings toPackageSettings(const OS &o)
{
    PackageSettings s;
    switch (o.Type)
    {
    case OSType::Windows:
        s["os"]["kernel"] = PackagePath{ "com.Microsoft.Windows.NT"s };
        break;
    case OSType::Linux:
        s["os"]["kernel"] = PackagePath{ "org.torvalds.linux"s };
        break;
    case OSType::Macos:
        s["os"]["kernel"] = PackagePath{ "com.Apple.Macos"s };
        break;
    case OSType::Darwin:
        s["os"]["kernel"] = PackagePath{ "com.Apple.Darwin"s };
        break;
    case OSType::Cygwin:
        s["os"]["kernel"] = PackagePath{ "org.cygwin"s };
        break;
    case OSType::Mingw:
        s["os"]["kernel"] = PackagePath{ "org.mingw"s };
        break;
    default:
        SW_UNIMPLEMENTED;
    }
    // do not specify, just takes max available
    //s["os"]["version"] = o.Version.toString();

    switch (o.Arch)
    {
    case ArchType::x86:
        s["os"]["arch"] = "x86"s;
        break;
    case ArchType::x86_64:
        s["os"]["arch"] = "x86_64"s;
        break;
    case ArchType::arm:
        s["os"]["arch"] = "arm"s;
        break;
    case ArchType::aarch64:
        s["os"]["arch"] = "aarch64"s;
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
        s["os"]["environment"] = "gnueabi"s;
        break;
    case EnvironmentType::GNUEABIHF:
        s["os"]["environment"] = "gnueabihf"s;
        break;
    }

    // we might not have sdk version installed
    //s["os"]["version"] = o.Version.toString();

    return s;
}

} // namespace sw
