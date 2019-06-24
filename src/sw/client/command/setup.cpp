// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "commands.h"
#include "../inserts.h"

#include <boost/dll.hpp>
#include <primitives/win32helpers.h>

#ifdef _WIN32
#include <WinReg.hpp>
#endif

static void registerCmakePackage(sw::SwContext &swctx)
{
#ifdef _WIN32
    auto dir = swctx.getLocalStorage().storage_dir_etc / "sw" / "static";
    // if we write into HKLM, we won't be able to access the pkg file in admins folder
    winreg::RegKey icon(/*is_elevated() ? HKEY_LOCAL_MACHINE : */HKEY_CURRENT_USER, L"Software\\Kitware\\CMake\\Packages\\SW");
    icon.SetStringValue(L"", dir.wstring().c_str());
    write_file_if_different(dir / "SWConfig.cmake", sw_config_cmake);
#else
    auto cppan_cmake_dir = get_home_directory() / ".cmake" / "packages";
    write_file_if_different(cppan_cmake_dir / "SW" / "1", cppan_cmake_dir.string());
    write_file_if_different(cppan_cmake_dir / cppan_cmake_config_filename, cppan_cmake_config);
#endif
}

SUBCOMMAND_DECL(setup)
{
    elevate();

#ifdef _WIN32
    auto prog = boost::dll::program_location().wstring();

    // set common environment variable
    //winreg::RegKey env(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment");
    //env.SetStringValue(L"SW_TOOL", boost::dll::program_location().wstring());

    // set up protocol handler
    {
        const std::wstring id = L"sw";

        winreg::RegKey url(HKEY_CLASSES_ROOT, id);
        url.SetStringValue(L"URL Protocol", L"");

        winreg::RegKey icon(HKEY_CLASSES_ROOT, id + L"\\DefaultIcon");
        icon.SetStringValue(L"", prog);

        winreg::RegKey open(HKEY_CLASSES_ROOT, id + L"\\shell\\open\\command");
        open.SetStringValue(L"", prog + L" uri %1");
    }

    // register .sw extension
    {
        const std::wstring id = L"sw.1";

        winreg::RegKey ext(HKEY_CLASSES_ROOT, L".sw");
        ext.SetStringValue(L"", id);

        winreg::RegKey icon(HKEY_CLASSES_ROOT, id + L"\\DefaultIcon");
        icon.SetStringValue(L"", prog);

        winreg::RegKey p(HKEY_CLASSES_ROOT, id + L"\\shell\\open\\command");
        p.SetStringValue(L"", prog + L" build %1");
    }
#endif

    auto swctx = createSwContext();
    registerCmakePackage(*swctx);
}
