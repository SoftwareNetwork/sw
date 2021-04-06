// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include "detect.h"

#include "../command.h"
#include "../program_version_storage.h"

#include <boost/algorithm/string.hpp>

#include <regex>
#include <string>

#ifdef _WIN32
#include <windows.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "compiler.detect.win.sdk");

#include <WinReg.hpp>

// https://en.wikipedia.org/wiki/Microsoft_Windows_SDK
static const Strings known_kits{ "8.1A", "8.1", "8.0", "7.1A", "7.1", "7.0A", "7.0", "6.0A" };
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
                auto &t = sw::addTarget<sw::PredefinedTarget>(DETECT_ARGS_PASS, sw::LocalPackage(s.getLocalStorage(), sw::PackageId("com.Microsoft.Windows.SDK." + name, v)), ts);
                //t.ts["os"]["version"] = v.toString();

                t.public_ts["properties"]["6"]["system_include_directories"].push_back(idir / name);
                for (auto &i : idirs)
                    t.public_ts["properties"]["6"]["system_include_directories"].push_back(idir / i);
                t.public_ts["properties"]["6"]["system_link_directories"].push_back(libdir);
                targets.push_back(&t);
            }
            else if (without_ldir)
            {
                auto &t = sw::addTarget<sw::PredefinedTarget>(DETECT_ARGS_PASS, sw::LocalPackage(s.getLocalStorage(), sw::PackageId("com.Microsoft.Windows.SDK." + name, v)), ts);
                //t.ts["os"]["version"] = v.toString();

                t.public_ts["properties"]["6"]["system_include_directories"].push_back(idir / name);
                for (auto &i : idirs)
                    t.public_ts["properties"]["6"]["system_include_directories"].push_back(idir / i);
                targets.push_back(&t);
            }
            else
                LOG_TRACE(logger, "Libdir " << libdir << " not found for library: " << name);
        }
        return targets;
    }

    std::vector<sw::PredefinedTarget*> addOld(DETECT_ARGS, sw::OS settings, const sw::Version &v)
    {
        auto idir = kit_root / "Include";
        if (!fs::exists(idir))
        {
            LOG_TRACE(logger, "Include dir " << idir << " not found for kit: " << kit_root);
            return {};
        }

        std::vector<sw::PredefinedTarget *> targets;
        // only two archs
        // (but we have IA64 also)
        for (auto target_arch : { sw::ArchType::x86_64,sw::ArchType::x86 })
        {
            settings.Arch = target_arch;

            auto ts1 = toTargetSettings(settings);
            sw::TargetSettings ts;
            ts["os"]["kernel"] = ts1["os"]["kernel"];
            ts["os"]["arch"] = ts1["os"]["arch"];

            auto libdir = kit_root / "Lib";
            if (target_arch == sw::ArchType::x86_64)
                libdir /= toStringWindows(target_arch);
            if (fs::exists(libdir))
            {
                auto &t = sw::addTarget<sw::PredefinedTarget>(DETECT_ARGS_PASS, sw::LocalPackage(s.getLocalStorage(), sw::PackageId("com.Microsoft.Windows.SDK." + name, v)), ts);
                //t.ts["os"]["version"] = v.toString();

                t.public_ts["properties"]["6"]["system_include_directories"].push_back(idir);
                t.public_ts["properties"]["6"]["system_link_directories"].push_back(libdir);
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
            auto p = std::make_shared<sw::SimpleProgram>();
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
            auto p = std::make_shared<sw::SimpleProgram>();
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
            // old sdks
            p = d / "Microsoft SDKs";
            if (fs::exists(p))
                dirs.insert(p);
            p = d / "Microsoft SDKs" / "Windows"; // this is more correct probably
            if (fs::exists(p))
                dirs.insert(p);
        }
        return dirs;
    }

    static Files getWindowsKitRootFromReg(const std::wstring &key)
    {
        auto getWindowsKitRootFromReg = [](const std::wstring &root, const std::wstring &key, int access) -> path
        {
            winreg::RegKey kits;
            auto r = kits.TryOpen(HKEY_LOCAL_MACHINE, root, access);
            if (r.IsOk())
            {
                std::wstring result;
                auto r = kits.TryGetStringValue(L"KitsRoot" + key, result);
                if (r.IsOk())
                    return result;
                LOG_TRACE(logger, "getWindowsKitRootFromReg::TryGetStringValue error: "s + to_string(r.ErrorMessage()));
            }
            else
                LOG_TRACE(logger, "getWindowsKitRootFromReg::TryOpen error: "s + to_string(r.ErrorMessage()));
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
            winreg::RegKey kits10;
            auto r = kits10.TryOpen(HKEY_LOCAL_MACHINE, reg_root, access);
            if (r.IsOk())
            {
                std::vector<std::wstring> keys;
                auto r = kits10.TryEnumSubKeys(keys);
                if (r.IsOk())
                {
                    for (auto &k : keys)
                        kits.insert(to_string(k));
                    return;
                }
                LOG_TRACE(logger, "listWindows10KitsFromReg::TryEnumSubKeys error: "s + to_string(r.ErrorMessage()));
            }
            else
                LOG_TRACE(logger, "listWindows10KitsFromReg::TryOpen error: "s + to_string(r.ErrorMessage()));
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
                p = kr / ("v" + k); // some installations has v prefix
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
        LOG_TRACE(logger, "Found Windows Kit " + v.toString() + " at " + to_string(normalize_path(kr)));

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
                t->public_ts["properties"]["6"]["system_link_libraries"].push_back("KERNEL32.LIB");
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
        LOG_TRACE(logger, "Found Windows Kit " + k + " at " + to_string(normalize_path(kr)));

        // tools
        {
            WinKit wk;
            wk.kit_root = kr;
            wk.addTools(DETECT_ARGS_PASS);
        }

        auto ver = k;
        if (!k.empty() && k.back() == 'A')
            ver = k.substr(0, k.size() - 1) + ".1";

        // old kits has special handling
        // if (sw::Version(k) < sw::Version(8)) k may have letter 'A', so we can't use such cmp at the moment
        // use simple cmp for now
        // but we pass k as version, so when we need to handle X.XA, we must set a new way
        if (k == "7.1" || k == "7.1A")
        {
            WinKit wk;
            wk.kit_root = kr;
            wk.name = "um";
            wk.addOld(DETECT_ARGS_PASS, settings, ver);
            wk.name = "km"; // ? maybe km files installed separately?
            wk.addOld(DETECT_ARGS_PASS, settings, ver);
            return;
        }

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
            wk.add(DETECT_ARGS_PASS, settings, ver);
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
            wk.add(DETECT_ARGS_PASS, settings, ver);
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
