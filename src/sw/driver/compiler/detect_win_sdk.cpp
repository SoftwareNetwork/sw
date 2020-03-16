// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "detect.h"

#include "../command.h"
#include "../program_version_storage.h"

#include <boost/algorithm/string.hpp>

#include <regex>
#include <string>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "compiler.detect.win.sdk");

#ifdef _WIN32
#include <WinReg.hpp>

// https://en.wikipedia.org/wiki/Microsoft_Windows_SDK
static const Strings known_kits{ "8.1A", "8.1", "8.0", "7.1A", "7.1", "7.0A", "7.0A","6.0A" };
static const auto reg_root = L"SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots";
// list all registry views
static const int reg_access_list[] =
{
    KEY_READ,
    KEY_READ | KEY_WOW64_32KEY,
    KEY_READ | KEY_WOW64_64KEY
};
static const String win10_kit_name = "10";

namespace
{

struct WinKit
{
    path kit_root;

    String name;

    String bdir_subversion;
    String idir_subversion;
    String ldir_subversion;

    Strings idirs; // additional idirs
    bool without_ldir = false; // when there's not libs

    std::vector<sw::PredefinedTarget*> add(DETECT_ARGS, sw::OS settings, const sw::Version &v)
    {
        auto idir = kit_root / "Include" / idir_subversion;
        if (!fs::exists(idir / name))
        {
            LOG_TRACE(logger, "Include dir " << (idir / name) << " not found for library: " << name);
            return {};
        }

        std::vector<sw::PredefinedTarget *> targets;
        for (auto target_arch : { sw::ArchType::x86_64,sw::ArchType::x86,sw::ArchType::arm,sw::ArchType::aarch64 })
        {
            settings.Arch = target_arch;

            auto ts1 = toTargetSettings(settings);
            sw::TargetSettings ts;
            ts["os"]["kernel"] = ts1["os"]["kernel"];
            ts["os"]["arch"] = ts1["os"]["arch"];

            auto libdir = kit_root / "Lib" / ldir_subversion / name / toStringWindows(target_arch);
            if (fs::exists(libdir))
            {
                auto &t = sw::addTarget<sw::PredefinedTarget>(DETECT_ARGS_PASS, sw::PackageId("com.Microsoft.Windows.SDK." + name, v), ts);
                //t.ts["os"]["version"] = v.toString();

                t.public_ts["system_include_directories"].push_back(normalize_path(idir / name));
                for (auto &i : idirs)
                    t.public_ts["system_include_directories"].push_back(normalize_path(idir / i));
                t.public_ts["system_link_directories"].push_back(normalize_path(libdir));
                targets.push_back(&t);
            }
            else if (without_ldir)
            {
                auto &t = sw::addTarget<sw::PredefinedTarget>(DETECT_ARGS_PASS, sw::PackageId("com.Microsoft.Windows.SDK." + name, v), ts);
                //t.ts["os"]["version"] = v.toString();

                t.public_ts["system_include_directories"].push_back(normalize_path(idir / name));
                for (auto &i : idirs)
                    t.public_ts["system_include_directories"].push_back(normalize_path(idir / i));
                targets.push_back(&t);
            }
            else
                LOG_TRACE(logger, "Libdir " << libdir << " not found for library: " << name);
        }
        return targets;
    }

    void addTools(DETECT_ARGS)
    {
        // .rc
        {
            auto p = std::make_shared<sw::SimpleProgram>(s);
            p->file = kit_root / "bin" / bdir_subversion / toStringWindows(s.getHostOs().Arch) / "rc.exe";
            if (fs::exists(p->file))
            {
                auto v = getVersion(s, p->file, "/?");
                sw::TargetSettings ts2;
                auto ts1 = toTargetSettings(s.getHostOs());
                ts2["os"]["kernel"] = ts1["os"]["kernel"];
                auto &rc = addProgram(DETECT_ARGS_PASS, sw::PackageId("com.Microsoft.Windows.rc", v), ts2, p);
            }
            // these are passed from compiler during merge?
            //for (auto &idir : COpts.System.IncludeDirectories)
            //C->system_idirs.push_back(idir);
        }

        // .mc
        {
            auto p = std::make_shared<sw::SimpleProgram>(s);
            p->file = kit_root / "bin" / bdir_subversion / toStringWindows(s.getHostOs().Arch) / "mc.exe";
            if (fs::exists(p->file))
            {
                auto v = getVersion(s, p->file, "/?");
                auto ts1 = toTargetSettings(s.getHostOs());
                sw::TargetSettings ts2;
                ts2["os"]["kernel"] = ts1["os"]["kernel"];
                auto &rc = addProgram(DETECT_ARGS_PASS, sw::PackageId("com.Microsoft.Windows.mc", v), ts2, p);
            }
            // these are passed from compiler during merge?
            //for (auto &idir : COpts.System.IncludeDirectories)
            //C->system_idirs.push_back(idir);
        }
    }
};

struct WinSdkInfo
{
    sw::OS settings;

    WinSdkInfo()
    {
        default_sdk_roots = getDefaultSdkRoots();
        win10_sdk_roots = getWindowsKitRootFromReg(L"10");
        win81_sdk_roots = getWindowsKitRootFromReg(L"81");
    }

    void listWindowsKits(DETECT_ARGS)
    {
        // we have now possible double detections, but this is fine for now

        listWindows10Kits(DETECT_ARGS_PASS);
        listWindowsKitsOld(DETECT_ARGS_PASS);
    }

private:
    Files default_sdk_roots;
    Files win10_sdk_roots;
    Files win81_sdk_roots;

    static Files getProgramFilesDirs()
    {
        auto get_env = [](auto p) -> path
        {
            if (auto e = getenv(p))
                return e;
            return {};
        };

        Files dirs;
        for (auto p : { "ProgramFiles(x86)","ProgramFiles","ProgramW6432" })
        {
            if (auto e = get_env(p); !e.empty())
                dirs.insert(e);
        }
        if (dirs.empty())
            throw SW_RUNTIME_ERROR("Cannot get 'ProgramFiles/ProgramFiles(x86)/ProgramW6432' env. vars.");
        return dirs;
    }

    static Files getDefaultSdkRoots()
    {
        Files dirs;
        for (auto &d : getProgramFilesDirs())
        {
            auto p = d / "Windows Kits";
            if (fs::exists(p))
                dirs.insert(p);
        }
        return dirs;
    }

    static Files getWindowsKitRootFromReg(const std::wstring &key)
    {
        auto getWindowsKitRootFromReg = [](const std::wstring &root, const std::wstring &key, int access) -> path
        {
            try
            {
                // may throw if not installed
                winreg::RegKey kits(HKEY_LOCAL_MACHINE, root, access);
                return kits.GetStringValue(L"KitsRoot" + key);
            }
            catch (std::exception &e)
            {
                LOG_TRACE(logger, "getWindowsKitRootFromReg: "s + e.what());
            }
            return {};
        };

        Files dirs;
        for (auto k : reg_access_list)
        {
            auto p = getWindowsKitRootFromReg(reg_root, key, k);
            if (p.empty())
                continue;
            // in registry path are written like 'C:\\Program Files (x86)\\Windows Kits\\10\\'
            if (p.filename().empty())
                p = p.parent_path();
            dirs.insert(p);
        }
        return dirs;
    }

    static sw::VersionSet listWindows10KitsFromReg()
    {
        auto list_kits = [](auto &kits, int access)
        {
            try
            {
                winreg::RegKey kits10(HKEY_LOCAL_MACHINE, reg_root, access);
                for (auto &k : kits10.EnumSubKeys())
                    kits.insert(to_string(k));
            }
            catch (std::exception & e)
            {
                LOG_TRACE(logger, "listWindows10KitsFromReg: "s + e.what());
            }
        };

        sw::VersionSet kits;
        for (auto k : reg_access_list)
            list_kits(kits, k);
        return kits;
    }

    void listWindows10Kits(DETECT_ARGS) const
    {
        auto kits = listWindows10KitsFromReg();

        auto win10_roots = win10_sdk_roots;
        for (auto &d : default_sdk_roots)
        {
            auto p = d / win10_kit_name;
            if (fs::exists(p))
                win10_roots.insert(p);
        }

        // add more win10 kits
        for (auto &kr10 : win10_roots)
        {
            if (!fs::exists(kr10 / "Include"))
                continue;
            // also try directly (kit 10.0.10240 does not register in registry)
            for (auto &d : fs::directory_iterator(kr10 / "Include"))
            {
                auto k = d.path().filename().string();
                if (fs::exists(kr10 / "Lib" / k) && sw::Version(k).isVersion())
                    kits.insert(k);
            }
        }

        // now add all kits
        for (auto &kr10 : win10_roots)
        {
            for (auto &v : kits)
                add10Kit(DETECT_ARGS_PASS, kr10, v);
        }
    }

    void listWindowsKitsOld(DETECT_ARGS) const
    {
        for (auto &kr : win81_sdk_roots)
            addKit(DETECT_ARGS_PASS, kr, "8.1");

        for (auto &kr : default_sdk_roots)
        {
            for (auto &k : known_kits)
            {
                auto p = kr / k;
                if (fs::exists(p))
                    addKit(DETECT_ARGS_PASS, p, k);
            }
        }
    }

    //
    // ucrt - universal CRT
    //
    // um - user mode
    // km - kernel mode
    // shared - some of these and some of these
    //

    void add10Kit(DETECT_ARGS, const path &kr, const sw::Version &v) const
    {
        LOG_TRACE(logger, "Found Windows Kit " + v.toString() + " at " + normalize_path(kr));

        // ucrt
        {
            WinKit wk;
            wk.name = "ucrt";
            wk.kit_root = kr;
            wk.idir_subversion = v.toString();
            wk.ldir_subversion = v.toString();
            wk.add(DETECT_ARGS_PASS, settings, v);
        }

        // um + shared
        {
            WinKit wk;
            wk.name = "um";
            wk.kit_root = kr;
            wk.idir_subversion = v.toString();
            wk.ldir_subversion = v.toString();
            wk.idirs.push_back("shared");
            for (auto t : wk.add(DETECT_ARGS_PASS, settings, v))
                t->public_ts["system_link_libraries"].push_back("kernel32.lib");
        }

        // km
        {
            WinKit wk;
            wk.name = "km";
            wk.kit_root = kr;
            wk.idir_subversion = v.toString();
            wk.ldir_subversion = v.toString();
            wk.add(DETECT_ARGS_PASS, settings, v);
        }

        // winrt
        {
            WinKit wk;
            wk.name = "winrt";
            wk.kit_root = kr;
            wk.idir_subversion = v.toString();
            wk.without_ldir = true;
            wk.add(DETECT_ARGS_PASS, settings, v);
        }

        // tools
        {
            WinKit wk;
            wk.kit_root = kr;
            wk.bdir_subversion = v.toString();
            wk.addTools(DETECT_ARGS_PASS);
        }
    }

    void addKit(DETECT_ARGS, const path &kr, const String &k) const
    {
        LOG_TRACE(logger, "Found Windows Kit " + k + " at " + normalize_path(kr));

        // um + shared
        {
            WinKit wk;
            wk.name = "um";
            wk.kit_root = kr;
            if (k == "8.1")
                wk.ldir_subversion = "winv6.3";
            else if (k == "8.0")
                wk.ldir_subversion = "Win8";
            else
                LOG_DEBUG(logger, "TODO: Windows Kit " + k + " is not implemented yet. Report this issue.");
            wk.idirs.push_back("shared");
            wk.add(DETECT_ARGS_PASS, settings, k);
        }

        // km
        {
            WinKit wk;
            wk.name = "km";
            wk.kit_root = kr;
            if (k == "8.1")
                wk.ldir_subversion = "winv6.3";
            else if (k == "8.0")
                wk.ldir_subversion = "Win8";
            else
                LOG_DEBUG(logger, "TODO: Windows Kit " + k + " is not implemented yet. Report this issue.");
            wk.add(DETECT_ARGS_PASS, settings, k);
        }

        // tools
        {
            WinKit wk;
            wk.kit_root = kr;
            wk.addTools(DETECT_ARGS_PASS);
        }
    }
};

}

namespace sw
{

void detectWindowsSdk(DETECT_ARGS)
{
    WinSdkInfo info;
    info.settings = s.getHostOs();
    info.listWindowsKits(DETECT_ARGS_PASS);
}

}

#else
namespace sw { void detectWindowsSdk(DETECT_ARGS) {} } // noop
#endif // #ifdef _WIN32
