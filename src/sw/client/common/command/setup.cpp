/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2019 Egor Pugin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "commands.h"
#include "../inserts.h"

#include <sw/manager/storage.h>

#include <boost/dll.hpp>

#ifdef _WIN32
#include <primitives/win32helpers.h>
#include <WinReg.hpp>
#endif

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "setup");

static void registerCmakePackage(sw::SwContext &swctx)
{
    const auto sw_cmake_config_filename = "SWConfig.cmake";

    auto write_cmake = [&sw_cmake_config_filename](const path &dir)
    {
        auto sw_cmake_dir = dir / ".cmake" / "packages";
        write_file_if_different(sw_cmake_dir / "SW" / "1", sw_cmake_dir.u8string());
        write_file_if_different(sw_cmake_dir / sw_cmake_config_filename, sw_config_cmake);
    };

#ifdef _WIN32
    auto dir = swctx.getLocalStorage().storage_dir_etc / "sw" / "static";
    // if we write into HKLM, we won't be able to access the pkg file in admins folder
    winreg::RegKey icon(/*is_elevated() ? HKEY_LOCAL_MACHINE : */HKEY_CURRENT_USER, L"Software\\Kitware\\CMake\\Packages\\SW");
    icon.SetStringValue(L"", dir.wstring().c_str());
    write_file_if_different(dir / sw_cmake_config_filename, sw_config_cmake);

    // cygwin case
    if (auto d = getenv("HOME"))
        write_cmake(d);
#else
    write_cmake(get_home_directory());
#endif
}

SUBCOMMAND_DECL(setup)
{
#ifdef _WIN32
    elevate();

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
    // remove? improve?
    /*{
        const std::wstring id = L"sw.1";

        winreg::RegKey ext(HKEY_CLASSES_ROOT, L".sw");
        ext.SetStringValue(L"", id);

        winreg::RegKey icon(HKEY_CLASSES_ROOT, id + L"\\DefaultIcon");
        icon.SetStringValue(L"", prog);

        winreg::RegKey p(HKEY_CLASSES_ROOT, id + L"\\shell\\open\\command");
        p.SetStringValue(L"", prog + L" build %1");
    }*/

#elif defined(__linux__)
    // https://specifications.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html
    // Exec=)"s + normalize_path(prog) + R"( uri %u
    // or
    // Exec=sw uri %u
    const auto opener = R"([Desktop Entry]
Type=Application
Name=SW Scheme Handler
Exec=sw uri %u
StartupNotify=false
Terminal=true
MimeType=x-scheme-handler/sw;
)"s;
    const auto fn = "sw-opener.desktop"s;
    write_file(get_home_directory() / ".local/share/applications" / fn, opener);
    String cmd;
    cmd = "xdg-mime default " + fn + " x-scheme-handler/sw";
    if (system(cmd.c_str()))
        LOG_ERROR(logger, "Cannot register sw uri handler");

#elif defined(__APPLE__)
#endif

    auto swctx = createSwContext(options);
    registerCmakePackage(*swctx);
}
