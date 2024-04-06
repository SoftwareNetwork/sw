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
#include <ShObjIdl.h>
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
        auto prog = boost::dll::program_location().wstring();
        // i_am_not_ignoring_return_value
        winreg::RegResult r;

        // protocol handler
        winreg::RegKey url(HKEY_CLASSES_ROOT);
        r = url.TryDeleteTree(get_sw_registry_key());

        // cmake
        winreg::RegKey icon(HKEY_CURRENT_USER);
        r = icon.TryDeleteTree(get_sw_cmake_registry_key()); // remove all empty cmake keys and trees?

        // delete .sw
        winreg::RegKey _id(HKEY_CLASSES_ROOT);
        r = _id.TryDeleteTree(L"."s + get_sw_registry_key());

        // delete sw.1
        winreg::RegKey id1(HKEY_CLASSES_ROOT);
        r = id1.TryDeleteTree(get_sw_registry_key() + L".1"s);

        // delete SystemFileAssociations/.sw
        winreg::RegKey k(HKEY_CLASSES_ROOT, L"SystemFileAssociations");
        r = k.TryDeleteTree(L"."s + get_sw_registry_key());

        // delete from Path
        {
            winreg::RegKey url(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment");
            auto v = url.GetExpandStringValue(L"Path");
            boost::replace_all(v, prog, L"");
            boost::replace_all(v, L";;", L";");
            if (v.ends_with(';')) {
                v.resize(v.size() - 1);
            }
            url.SetExpandStringValue(L"Path", v);
        }

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
    // TODO: generate .com file for single-sw-file-gui?

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
    // insecure? ok?
    // to add .sw ext in VS - Tools | Options | Text Editor | File Extension | Microsoft Visual C++
    {
        auto id = get_sw_registry_key();
        auto id1 = id + L".1";
        auto _id = L"." + id;
        auto base_command = prog;
        auto end = L" %1 %*"s;
        auto make_command = [&](auto && ... args) {
            auto c = base_command;
            ((c += L" "s + args),...);
            c += end;
            return c;
        };
        auto run_command = [&](auto && ... args) {
            auto c = base_command;
            ((c += L" "s + args),...);
            c += L" run";
            c += end;
            return c;
        };

        winreg::RegKey ext(HKEY_CLASSES_ROOT, L"."s + id);
        ext.SetStringValue(L"", id1);

        winreg::RegKey icon(HKEY_CLASSES_ROOT, id1 + L"\\DefaultIcon");
        icon.SetStringValue(L"", prog);

        // we run these files & pause on exit, so user could check what went wrong
        winreg::RegKey p(HKEY_CLASSES_ROOT, id1 + L"\\shell\\open\\command");
        p.SetStringValue(L"", run_command(L"-pause-on-exit -shell -config r -config-name r"));

        path shell_key = L"SystemFileAssociations";
        shell_key /= _id;

        {
            winreg::RegKey k(HKEY_CLASSES_ROOT);
            auto r = k.TryDeleteTree(shell_key);
        }

        // create context menu on .sw files
        auto add_submenu = [](const path &parent, auto &&name, auto &&text) {
            auto key = parent / "shell" / name;
            winreg::RegKey p(HKEY_CLASSES_ROOT, key.wstring());
            p.SetStringValue(L"MUIVerb", text);
            p.SetStringValue(L"subcommands", L"");
            return key;
        };
        auto add_item = [](const path &parent, auto &&name, auto &&text, auto &&cmd, int flags = 0) {
            auto key = parent / "shell" / name;
            winreg::RegKey p(HKEY_CLASSES_ROOT, key.wstring());
            p.SetStringValue(L"MUIVerb", text);
            if (flags) {
                winreg::RegKey p(HKEY_CLASSES_ROOT, key.wstring());
                p.SetDwordValue(L"CommandFlags", flags);
            }
            winreg::RegKey p2(HKEY_CLASSES_ROOT, (key / "command").wstring());
            p2.SetStringValue(L"", cmd);
            return key;
        };
        {
            auto sw = add_submenu(shell_key, id, id);
            // add icon
            {
                winreg::RegKey k(HKEY_CLASSES_ROOT, sw);
                k.SetStringValue(L"icon", prog);
            }

            auto f = [&](auto &&parent, std::wstring cmd) {
                auto f2 = [&](auto &&parent, std::wstring cmd) {
                    add_item(parent, L"1_debug", L"Debug", make_command(cmd + L" -config d -config-name d"));
                    // currently we are out of limit on the shell items,
                    // so we need to remove some of them or create shell extension
                    //add_item(parent, L"2_rwdi", L"RelWithDebInfo", make_command(cmd + L" -config rwdi -config-name rwdi"));
                    add_item(parent, L"3_r", L"Release", make_command(cmd + L" -config r -config-name r"));
                };
                auto shared = add_submenu(parent, L"shared", L"Shared");
                //auto default_str = L"Debug,RelWithDebInfo,Release";
                auto default_str = L"Default";
                if (cmd == L"generate") {
                    add_item(shared, L"0_d_rwdi_r", default_str, make_command(cmd + L" -config d,rwdi,r -config-name d,rwdi,r"), ECF_SEPARATORAFTER);
                }
                f2(shared, cmd + L" -shared");
                auto static_ = add_submenu(parent, L"static", L"Static");
                if (cmd == L"generate") {
                    add_item(static_, L"0_d_rwdi_r", default_str, make_command(cmd + L" -static -config d,rwdi,r -config-name static_d,static_rwdi,static_r"), ECF_SEPARATORAFTER);
                }
                f2(static_, cmd + L" -static");
            };
            auto generate = add_submenu(sw, L"generate", L"Generate");
            f(generate, L" -pause-on-error generate");
            auto run = add_submenu(sw, L"run", L"Run");
            f(run, L" -pause-on-exit -shell run"); // we use shell arg here to change working dir to storage dir
        }
    }

    if (getOptions().options_setup.add_to_path)
    {
        winreg::RegKey url(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment");
        auto v = url.GetExpandStringValue(L"Path");
        auto p = path{prog}.parent_path().wstring();
        if (!v.ends_with(';')) {
            v += L";";
        }
        v += p;
        url.SetExpandStringValue(L"Path", v);
    }

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
