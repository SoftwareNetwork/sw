// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include "detect.h"

#include "rc.h"
#include "../rule.h"
#include "../build.h"
#include "../command.h"
#include "../program_version_storage.h"

#include <boost/algorithm/string.hpp>

#include <regex>
#include <string>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "compiler.detect.win.sdk");

#ifdef _WIN32
#include <WinReg.hpp>

using DetectablePackageMultiEntryPoints = sw::ProgramDetector::DetectablePackageMultiEntryPoints;

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
    sw::PackageVersion v;
    path kit_root;

    String name;

    String bdir_subversion;
    String idir_subversion;
    String ldir_subversion;

    Strings idirs; // additional idirs
    bool without_ldir = false; // when there's not libs

    DetectablePackageMultiEntryPoints add()
    {
        DetectablePackageMultiEntryPoints eps;
        auto tname = "com.Microsoft.Windows.SDK." + name;

        eps.emplace(tname, [this, tname](DETECT_ARGS)
        {
            auto idir = kit_root / "Include" / idir_subversion;
            if (!fs::exists(idir / name))
            {
                LOG_TRACE(logger, "Include dir " << (idir / name) << " not found for library: " << name);
                return;
            }

            auto &eb = static_cast<sw::ExtendedBuild &>(b);
            sw::BuildSettings new_settings = eb.getSettings();
            const auto target_arch = new_settings.TargetOS.Arch;

            auto libdir = kit_root / "Lib" / ldir_subversion / name / toStringWindows(target_arch);
            sw::PredefinedTarget *target = nullptr;
            if (fs::exists(libdir))
            {
                auto &t = sw::ProgramDetector::addTarget<sw::PredefinedTarget>(DETECT_ARGS_PASS,
                    sw::PackageName(tname, v), eb.getSettings());
                t.public_ts["properties"]["6"]["system_include_directories"].push_back(idir / name);
                for (auto &i : idirs)
                    t.public_ts["properties"]["6"]["system_include_directories"].push_back(idir / i);
                t.public_ts["properties"]["6"]["system_link_directories"].push_back(libdir);
                target = &t;
            }
            else if (without_ldir)
            {
                auto &t = sw::ProgramDetector::addTarget<sw::PredefinedTarget>(DETECT_ARGS_PASS,
                    sw::PackageName(tname, v), eb.getSettings());
                t.public_ts["properties"]["6"]["system_include_directories"].push_back(idir / name);
                for (auto &i : idirs)
                    t.public_ts["properties"]["6"]["system_include_directories"].push_back(idir / i);
                target = &t;
            }
            else
            {
                LOG_TRACE(logger, "Libdir " << libdir << " not found for library: " << name);
                return;
            }
            if (name == "um")
                target->public_ts["properties"]["6"]["system_link_libraries"].push_back(path(boost::to_upper_copy("kernel32.lib"s)));
            if (name == "ucrt")
                target->public_ts["properties"]["6"]["system_link_libraries"].push_back(
                    path(boost::to_upper_copy(sw::getProgramDetector().getMsvcLibraryName("ucrt", new_settings))));
        });

        return eps;
    }

    DetectablePackageMultiEntryPoints addTools()
    {
        DetectablePackageMultiEntryPoints eps;

        // .rc
        eps.emplace("com.Microsoft.Windows.rc", [this](DETECT_ARGS)
        {
            auto &eb = static_cast<sw::ExtendedBuild &>(b);
            auto p = std::make_unique<sw::RcTool>();
            p->file = kit_root / "bin" / bdir_subversion / toStringWindows(b.getContext().getHostOs().Arch) / "rc.exe";
            if (fs::exists(p->file))
            {
                auto v = getVersion(b.getContext(), p->file, "/?");
                auto &rc = sw::ProgramDetector::addProgram(DETECT_ARGS_PASS, sw::PackageName("com.Microsoft.Windows.rc", v), eb.getSettings(), *p);
                rc.setRule("rc", std::make_unique<sw::RcRule>(std::move(p)));
            }
        });

        // .mc
        eps.emplace("com.Microsoft.Windows.mc", [this](DETECT_ARGS)
        {
            SW_UNIMPLEMENTED;
            auto &eb = static_cast<sw::ExtendedBuild &>(b);
            auto p = std::make_unique<sw::SimpleProgram>(); // TODO: needs proper rule
            p->file = kit_root / "bin" / bdir_subversion / toStringWindows(b.getContext().getHostOs().Arch) / "mc.exe";
            if (fs::exists(p->file))
            {
                auto v = getVersion(b.getContext(), p->file, "/?");
                auto &rc = sw::ProgramDetector::addProgram(DETECT_ARGS_PASS, sw::PackageName("com.Microsoft.Windows.mc", v), eb.getSettings(), *p);
                rc.setRule("mc", std::make_unique<sw::RcRule>(std::move(p)));
            }
        });

        return eps;
    }
};

struct WinSdkInfo
{
    WinSdkInfo()
    {
        default_sdk_roots = getDefaultSdkRoots();
        win10_sdk_roots = getWindowsKitRootFromReg(L"10");
        win81_sdk_roots = getWindowsKitRootFromReg(L"81");
        listWindowsKits();
    }

    DetectablePackageMultiEntryPoints addWindowsKits()
    {
        DetectablePackageMultiEntryPoints eps;
        for (auto &[_,k] : libs)
            eps.merge(k.add());
        for (auto &[_,k] : programs)
            eps.merge(k.addTools());
        return eps;
    }

private:
    Files default_sdk_roots;
    Files win10_sdk_roots;
    Files win81_sdk_roots;
    mutable std::multimap<String, WinKit> libs;
    mutable std::multimap<String, WinKit> programs;

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

    void listWindowsKits()
    {
        // we have now possible double detections, but this is fine for now

        listWindows10Kits();
        listWindowsKitsOld();
    }

    void listWindows10Kits() const
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
                if (fs::exists(kr10 / "Lib" / k) && sw::PackageVersion(k).isVersion())
                    kits.insert(k);
            }
        }

        // now add all kits
        for (auto &kr10 : win10_roots)
        {
            for (auto &v : kits)
                add10Kit(kr10, v);
        }
    }

    void listWindowsKitsOld() const
    {
        for (auto &kr : win81_sdk_roots)
            addKit(kr, "8.1");

        for (auto &kr : default_sdk_roots)
        {
            for (auto &k : known_kits)
            {
                auto p = kr / k;
                if (fs::exists(p))
                    addKit(p, k);
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

    void add10Kit(const path &kr, const sw::PackageVersion &v) const
    {
        LOG_TRACE(logger, "Found Windows Kit " + v.toString() + " at " + to_string(normalize_path(kr)));

        // ucrt
        {
            WinKit wk;
            wk.v = v;
            wk.name = "ucrt";
            wk.kit_root = kr;
            wk.idir_subversion = v.toString();
            wk.ldir_subversion = v.toString();
            libs.emplace(wk.name, wk);
        }

        // um + shared
        {
            WinKit wk;
            wk.v = v;
            wk.name = "um";
            wk.kit_root = kr;
            wk.idir_subversion = v.toString();
            wk.ldir_subversion = v.toString();
            wk.idirs.push_back("shared");
            libs.emplace(wk.name, wk);
        }

        // km
        {
            WinKit wk;
            wk.v = v;
            wk.name = "km";
            wk.kit_root = kr;
            wk.idir_subversion = v.toString();
            wk.ldir_subversion = v.toString();
            libs.emplace(wk.name, wk);
        }

        // winrt
        {
            WinKit wk;
            wk.v = v;
            wk.name = "winrt";
            wk.kit_root = kr;
            wk.idir_subversion = v.toString();
            wk.without_ldir = true;
            libs.emplace(wk.name, wk);
        }

        // tools
        {
            WinKit wk;
            wk.v = v;
            wk.kit_root = kr;
            wk.bdir_subversion = v.toString();
            programs.emplace(wk.name, wk);
        }
    }

    void addKit(const path &kr, const String &k) const
    {
        LOG_TRACE(logger, "Found Windows Kit " + k + " at " + to_string(normalize_path(kr)));

        // um + shared
        {
            WinKit wk;
            wk.v = k;
            wk.name = "um";
            wk.kit_root = kr;
            if (k == "8.1")
                wk.ldir_subversion = "winv6.3";
            else if (k == "8.0")
                wk.ldir_subversion = "Win8";
            else
                LOG_DEBUG(logger, "TODO: Windows Kit " + k + " is not implemented yet. Report this issue.");
            wk.idirs.push_back("shared");
            libs.emplace(wk.name, wk);
        }

        // km
        {
            WinKit wk;
            wk.v = k;
            wk.name = "km";
            wk.kit_root = kr;
            if (k == "8.1")
                wk.ldir_subversion = "winv6.3";
            else if (k == "8.0")
                wk.ldir_subversion = "Win8";
            else
                LOG_DEBUG(logger, "TODO: Windows Kit " + k + " is not implemented yet. Report this issue.");
            libs.emplace(wk.name, wk);
        }

        // tools
        {
            WinKit wk;
            wk.v = k;
            wk.kit_root = kr;
            programs.emplace(wk.name, wk);
        }
    }
};

}
#endif // #ifdef _WIN32

namespace sw
{

ProgramDetector::DetectablePackageMultiEntryPoints ProgramDetector::detectWindowsSdk()
{
#ifdef _WIN32
    static WinSdkInfo info; // TODO: move into program detector later
    return info.addWindowsKits();
#endif
    return {};
}

}
