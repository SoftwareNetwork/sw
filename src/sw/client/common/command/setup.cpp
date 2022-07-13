// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>

#include "../commands.h"
#include "../inserts.h"
#include "../sw_context.h"

#include <sw/manager/storage.h>

#include <boost/dll.hpp>

#ifdef _WIN32
#include <primitives/win32helpers.h>
#include <WinReg.hpp>
#endif

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "setup");

const String &getCmakeConfig()
{
    return sw_config_cmake;
}

static std::wstring get_sw_registry_key()
{
    return L"sw";
}

static std::wstring get_sw_cmake_registry_key()
{
    return L"Software\\Kitware\\CMake\\Packages\\SW";
}

static path get_sw_linux_scheme_handler()
{
    return get_home_directory() / ".local/share/applications" / "sw-opener.desktop";
}

static path get_cmake_dir(const path &root)
{
    return root / ".cmake" / "packages";
}

static String get_cmake_dir_name()
{
    return "SW";
}

static String get_sw_cmake_config_filename()
{
    return "SWConfig.cmake";
}

static void registerCmakePackage(SwClientContext &swctx)
{
    auto write_cmake = [](const path &dir)
    {
        auto sw_cmake_dir = get_cmake_dir(dir);
        write_file_if_different(sw_cmake_dir / get_cmake_dir_name() / "1", to_string(sw_cmake_dir.u8string()));
        write_file_if_different(sw_cmake_dir / get_sw_cmake_config_filename(), getCmakeConfig());
    };

#ifdef _WIN32
    auto dir = swctx.getContext(false).getLocalStorage().storage_dir_etc / "sw" / "static";
    // if we write into HKLM, we won't be able to access the pkg file in admins folder
    winreg::RegKey icon(/*is_elevated() ? HKEY_LOCAL_MACHINE : */HKEY_CURRENT_USER, get_sw_cmake_registry_key());
    icon.SetStringValue(L"", dir.wstring().c_str());
    write_file_if_different(dir / get_sw_cmake_config_filename(), getCmakeConfig());

    // cygwin case
    if (auto d = getenv("HOME"))
        write_cmake(d);
#else
    write_cmake(get_home_directory());
#endif
}

enum CleanMask
{
    CLEAN_STORAGE           = 1,
    CLEAN_SYSTEM_SETTINGS   = 2,
    CLEAN_SETTINGS          = 4,
    CLEAN_EXECUTABLE        = 8,
};

static void cleanup(CleanMask level_mask, const sw::SwContext &swctx)
{
    std::error_code ec;

    if (level_mask & CLEAN_STORAGE)
    {
        // maybe add remove or cleanup method to Directories?
        fs::remove_all(swctx.getLocalStorage().storage_dir);
    }
    if (level_mask & CLEAN_SYSTEM_SETTINGS)
    {
#ifdef _WIN32
        // protocol handler
        winreg::RegKey url(HKEY_CLASSES_ROOT);
        url.TryDeleteTree(get_sw_registry_key());
        // cmake
        winreg::RegKey icon(HKEY_CURRENT_USER);
        icon.TryDeleteTree(get_sw_cmake_registry_key()); // remove all empty cmake keys and trees?

        // cygwin case
        if (auto d = getenv("HOME"))
        {
            fs::remove(get_cmake_dir(d), ec);
            fs::remove(get_cmake_dir(d) / get_sw_cmake_config_filename(), ec);
        }
#else
        fs::remove(get_sw_linux_scheme_handler(), ec);
        // cmake
        fs::remove(get_cmake_dir(get_home_directory()), ec);
        fs::remove(get_cmake_dir(get_home_directory()) / get_sw_cmake_config_filename(), ec);
#endif
    }
    if (level_mask & CLEAN_SETTINGS)
    {
        fs::remove_all(sw::support::get_root_directory());
    }
    if (level_mask & CLEAN_EXECUTABLE)
    {
        auto prog = boost::dll::program_location().u8string();
        String fn = to_string(prog);
#ifdef _WIN32
        fn = "timeout /t 3 && del \"" + fn + "\"";
        const char *cmd = "cmd";
        const char *args[] =
        {
            cmd,
            "/c",
            fn.c_str(),
            nullptr,
        };
        execve(cmd, (char* const*)args, 0);
#else
        const char *cmd = "rm";
        const char *args[] =
        {
            cmd,
            fn.c_str(),
            nullptr,
        };
        const char *newenviron[] = { nullptr };
        execve(cmd, (char*const*)args, (char*const*)newenviron);
#endif
    }
}

static void cleanup(SwClientContext &swctx)
{
    int level = 0;
    for (auto &l : swctx.getOptions().options_setup.level)
        level |= l;
    if (level == 0)
        level = CLEAN_STORAGE | CLEAN_SYSTEM_SETTINGS;
    cleanup((CleanMask)level, swctx.getContext(false));
}

SUBCOMMAND_DECL(setup)
{
#ifdef _WIN32
    // also register for current user
    if (!is_elevated())
        registerCmakePackage(*this);

    elevate();
#endif

    if (getOptions().options_setup.uninstall)
    {
        cleanup(*this);
        return;
    }

#ifdef _WIN32
    auto prog = boost::dll::program_location().wstring();

    // set common environment variable
    //winreg::RegKey env(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment");
    //env.SetStringValue(L"SW_TOOL", boost::dll::program_location().wstring());

    // set up protocol handler
    {
        auto id = get_sw_registry_key();

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
    write_file(get_sw_linux_scheme_handler(), opener);
    String cmd;
    cmd = "xdg-mime default " + get_sw_linux_scheme_handler().filename().string() + " x-scheme-handler/sw";
    if (system(cmd.c_str()))
        LOG_ERROR(logger, "Cannot register sw uri handler");
#elif defined(__APPLE__)
#endif

    registerCmakePackage(*this);
}
