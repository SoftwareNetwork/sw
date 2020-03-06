// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "detect.h"

#include "../misc/cmVSSetupHelper.h"
#include "../command.h"
#include "../program_version_storage.h"

#include <boost/algorithm/string.hpp>
#ifdef _WIN32
#include <WinReg.hpp>
#endif

#include <regex>
#include <string>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "compiler.detect");

// TODO: actually detect.cpp may be rewritten as entry point

namespace sw
{

const StringSet &getCppHeaderFileExtensions()
{
    static const StringSet header_file_extensions{
        ".h",
        ".hh",
        ".hm",
        ".hpp",
        ".hxx",
        ".tcc",
        ".h++",
        ".H++",
        ".HPP",
        ".H",
    };
    return header_file_extensions;
}

const StringSet &getCppSourceFileExtensions()
{
    static const StringSet cpp_source_file_extensions{
        ".cc",
        ".CC",
        ".cpp",
        ".cp",
        ".cxx",
        //".ixx", // msvc modules?
        // cppm - clang?
        // mxx, mpp - build2?
        ".c++",
        ".C++",
        ".CPP",
        ".CXX",
        ".C", // old ext (Wt)
        // Objective-C
        ".m",
        ".mm",
    };
    return cpp_source_file_extensions;
}

void log_msg_detect_target(const String &m)
{
    LOG_TRACE(logger, m);
}

PredefinedProgramTarget &addProgram(DETECT_ARGS, const PackageId &id, const TargetSettings &ts, const std::shared_ptr<Program> &p)
{
    auto &t = addTarget<PredefinedProgramTarget>(s, id, ts);
    t.public_ts["output_file"] = normalize_path(p->file);
    t.setProgram(p);
    LOG_TRACE(logger, "Detected program: " + p->file.u8string());
    return t;
}

bool isCppHeaderFileExtension(const String &e)
{
    auto &exts = getCppHeaderFileExtensions();
    return exts.find(e) != exts.end();
}

bool isCppSourceFileExtensions(const String &e)
{
    auto &exts = getCppSourceFileExtensions();
    return exts.find(e) != exts.end();
}

VSInstances &gatherVSInstances(DETECT_ARGS)
{
    static VSInstances instances = [&s]()
    {
        VSInstances instances;
#ifdef _WIN32
        cmVSSetupAPIHelper h;
        h.EnumerateVSInstances();
        for (auto &i : h.instances)
        {
            path root = i.VSInstallLocation;
            Version v = to_string(i.Version);

            // actually, it does not affect cl.exe or other tool versions
            if (i.VSInstallLocation.find(L"Preview") != std::wstring::npos)
                v = v.toString() + "-preview";

            VSInstance inst;
            inst.root = root;
            inst.version = v;
            instances.emplace(v, inst);
        }
#endif
        return instances;
    }();
    return instances;
}

void detectMsvc15Plus(DETECT_ARGS)
{
    // https://docs.microsoft.com/en-us/cpp/c-runtime-library/crt-library-features?view=vs-2019

    auto &instances = gatherVSInstances(s);
    const auto host = toStringWindows(s.getHostOs().Arch);
    auto new_settings = s.getHostOs();

    for (auto target_arch : {ArchType::x86_64,ArchType::x86,ArchType::arm,ArchType::aarch64})
    {
        new_settings.Arch = target_arch;

        auto ts1 = toTargetSettings(new_settings);
        TargetSettings ts;
        ts["os"]["kernel"] = ts1["os"]["kernel"];
        ts["os"]["arch"] = ts1["os"]["arch"];

        for (auto &[_, instance] : instances)
        {
            auto root = instance.root / "VC";
            auto v = instance.version;

            if (v.getMajor() < 15)
            {
                // continue; // instead of throw ?
                throw SW_RUNTIME_ERROR("VS < 15 must be handled differently");
            }

            root = root / "Tools" / "MSVC" / boost::trim_copy(read_file(root / "Auxiliary" / "Build" / "Microsoft.VCToolsVersion.default.txt"));
            auto idir = root / "include";

            // get suffix
            auto target = toStringWindows(target_arch);

            auto compiler = root / "bin";
            auto host_root = compiler / ("Host" + host) / host;

            compiler /= path("Host" + host) / target;

            // VS programs inherit cl.exe version (V)
            // same for VS libs
            // because ml,ml64,lib,link version (O) has O.Major = V.Major - 5
            // e.g., V = 19.21..., O = 14.21.... (19 - 5 = 14)

            String msvc_prefix;

            // C, C++
            {
                auto p = std::make_shared<SimpleProgram>(s);
                p->file = compiler / "cl.exe";
                if (fs::exists(p->file))
                {
                    auto c = p->getCommand();
                    if (s.getHostOs().Arch != target_arch)
                        c->addPathDirectory(host_root);
                    msvc_prefix = detectMsvcPrefix(*c, idir);
                    // run getVersion via prepared command
                    builder::detail::ResolvableCommand c2 = *c;
                    v = getVersion(s, c2);
                    if (instance.version.isPreRelease())
                        v.getExtra() = instance.version.getExtra();
                    auto &cl = addProgram(s, PackageId("com.Microsoft.VisualStudio.VC.cl", v), ts, p);
                }
            }

            // lib, link
            {
                auto p = std::make_shared<SimpleProgram>(s);
                p->file = compiler / "link.exe";
                if (fs::exists(p->file))
                    addProgram(s, PackageId("com.Microsoft.VisualStudio.VC.link", v), ts, p);

                if (s.getHostOs().Arch != target_arch)
                {
                    auto c = p->getCommand();
                    c->addPathDirectory(host_root);
                }

                p = std::make_shared<SimpleProgram>(s);
                p->file = compiler / "lib.exe";
                if (fs::exists(p->file))
                    addProgram(s, PackageId("com.Microsoft.VisualStudio.VC.lib", v), ts, p);

                if (s.getHostOs().Arch != target_arch)
                {
                    auto c = p->getCommand();
                    c->addPathDirectory(host_root);
                }
            }

            // ASM
            if (target_arch == ArchType::x86_64 || target_arch == ArchType::x86)
            {
                auto p = std::make_shared<SimpleProgram>(s);
                p->file = compiler / (target_arch == ArchType::x86_64 ? "ml64.exe" : "ml.exe");
                if (fs::exists(p->file))
                {
                    addProgram(s, PackageId("com.Microsoft.VisualStudio.VC.ml", v), ts, p);
                    getMsvcIncludePrefixes()[p->file] = msvc_prefix;
                }
            }

            // dumpbin
            {
                auto p = std::make_shared<SimpleProgram>(s);
                p->file = compiler / "dumpbin.exe";
                if (fs::exists(p->file))
                {
                    auto c = p->getCommand();
                    // run getVersion via prepared command
                    builder::detail::ResolvableCommand c2 = *c;
                    auto v = getVersion(s, c2);
                    if (instance.version.isPreRelease())
                        v.getExtra() = instance.version.getExtra();
                    addProgram(s, PackageId("com.Microsoft.VisualStudio.VC.dumpbin", v), ts, p);
                }
            }

            // libc++
            {
                auto &libcpp = addTarget<PredefinedTarget>(s, PackageId("com.Microsoft.VisualStudio.VC.libcpp", v), ts);
                libcpp.public_ts["system_include_directories"].push_back(normalize_path(idir));
                libcpp.public_ts["system_link_directories"].push_back(normalize_path(root / "lib" / target));

                if (fs::exists(root / "ATLMFC" / "include"))
                {
                    auto &atlmfc = addTarget<PredefinedTarget>(s, PackageId("com.Microsoft.VisualStudio.VC.ATLMFC", v), ts);
                    atlmfc.public_ts["system_include_directories"].push_back(normalize_path(root / "ATLMFC" / "include"));
                    atlmfc.public_ts["system_link_directories"].push_back(normalize_path(root / "ATLMFC" / "lib" / target));
                }
            }

            // concrt
            if (fs::exists(root / "crt" / "src" / "concrt"))
            {
                auto &libcpp = addTarget<PredefinedTarget>(s, PackageId("com.Microsoft.VisualStudio.VC.concrt", v), ts);
                libcpp.public_ts["system_include_directories"].push_back(normalize_path(root / "crt" / "src" / "concrt"));
            }

            // vcruntime
            if (fs::exists(root / "crt" / "src" / "vcruntime"))
            {
                auto &libcpp = addTarget<PredefinedTarget>(s, PackageId("com.Microsoft.VisualStudio.VC.runtime", v), ts);
                libcpp.public_ts["system_include_directories"].push_back(normalize_path(root / "crt" / "src" / "vcruntime"));
            }
        }
    }
}

void detectMsvc14AndOlder(DETECT_ARGS)
{
    auto find_comn_tools = [](const Version &v) -> path
    {
        auto n = std::to_string(v.getMajor()) + std::to_string(v.getMinor());
        auto ver = "VS"s + n + "COMNTOOLS";
        auto e = getenv(ver.c_str());
        if (e)
        {
            String r = e;
            while (!r.empty() && (r.back() == '/' || r.back() == '\\'))
                r.pop_back();
            path root = r;
            root = root.parent_path().parent_path();
            return root;
        }
        return {};
    };

    auto toStringWindows14AndOlder = [](ArchType e)
    {
        switch (e)
        {
        case ArchType::x86_64:
            return "amd64";
        case ArchType::x86:
            return "x86";
        case ArchType::arm:
            return "arm";
        default:
            throw SW_RUNTIME_ERROR("Unknown Windows arch");
        }
    };

    auto new_settings = s.getHostOs();

    // no ArchType::aarch64?
    for (auto target_arch : { ArchType::x86_64,ArchType::x86,ArchType::arm, })
    {
        // following code is written using VS2015
        // older versions might need special handling

        new_settings.Arch = target_arch;

        auto ts1 = toTargetSettings(new_settings);
        TargetSettings ts;
        ts["os"]["kernel"] = ts1["os"]["kernel"];
        ts["os"]["arch"] = ts1["os"]["arch"];

        for (auto n : {14,12,11,10,9,8})
        {
            Version v(n);
            auto root = find_comn_tools(v);
            if (root.empty())
                continue;

            root /= "VC";
            auto idir = root / "include";

            // get suffix
            auto target = toStringWindows14AndOlder(target_arch);

            auto compiler = root / "bin";
            auto host_root = compiler;

            path libdir = "lib";
            libdir /= toStringWindows14AndOlder(target_arch);

            // VC/bin/ ... x86 files
            // VC/bin/amd64/ ... x86_64 files
            // VC/bin/arm/ ... arm files
            // so we need to add subdir for non x86 targets
            if (!s.getHostOs().is(ArchType::x86))
            {
                host_root /= toStringWindows14AndOlder(s.getHostOs().Arch);
            }

            // now set to root
            compiler = host_root;

            // VC/bin/x86_amd64
            // VC/bin/x86_arm
            // VC/bin/amd64_x86
            // VC/bin/amd64_arm
            if (s.getHostOs().Arch != target_arch)
            {
                //if (!s.getHostOs().is(ArchType::x86))
                compiler += "_"s + toStringWindows14AndOlder(target_arch);
            }

            // VS programs inherit cl.exe version (V)
            // same for VS libs
            // because ml,ml64,lib,link version (O) has O.Major = V.Major - 5
            // e.g., V = 19.21..., O = 14.21.... (19 - 5 = 14)

            String msvc_prefix;

            // C, C++
            {
                auto p = std::make_shared<SimpleProgram>(s);
                p->file = compiler / "cl.exe";
                if (fs::exists(p->file))
                {
                    auto c = p->getCommand();
                    if (s.getHostOs().Arch != target_arch)
                        c->addPathDirectory(host_root);
                    msvc_prefix = detectMsvcPrefix(*c, idir);
                    // run getVersion via prepared command
                    builder::detail::ResolvableCommand c2 = *c;
                    v = getVersion(s, c2);
                    addProgram(s, PackageId("com.Microsoft.VisualStudio.VC.cl", v), ts, p);
                }
                else
                    continue;
            }

            // lib, link
            {
                auto p = std::make_shared<SimpleProgram>(s);
                p->file = compiler / "link.exe";
                if (fs::exists(p->file))
                    addProgram(s, PackageId("com.Microsoft.VisualStudio.VC.link", v), ts, p);

                if (s.getHostOs().Arch != target_arch)
                {
                    auto c = p->getCommand();
                    c->addPathDirectory(host_root);
                }

                p = std::make_shared<SimpleProgram>(s);
                p->file = compiler / "lib.exe";
                if (fs::exists(p->file))
                    addProgram(s, PackageId("com.Microsoft.VisualStudio.VC.lib", v), ts, p);

                if (s.getHostOs().Arch != target_arch)
                {
                    auto c = p->getCommand();
                    c->addPathDirectory(host_root);
                }
            }

            // ASM
            if (target_arch == ArchType::x86_64 || target_arch == ArchType::x86)
            {
                auto p = std::make_shared<SimpleProgram>(s);
                p->file = compiler / (target_arch == ArchType::x86_64 ? "ml64.exe" : "ml.exe");
                if (fs::exists(p->file))
                {
                    auto &ml = addProgram(s, PackageId("com.Microsoft.VisualStudio.VC.ml", v), ts, p);
                    getMsvcIncludePrefixes()[p->file] = msvc_prefix;
                }
            }

            // dumpbin
            {
                auto p = std::make_shared<SimpleProgram>(s);
                p->file = compiler / "dumpbin.exe";
                if (fs::exists(p->file))
                {
                    auto c = p->getCommand();
                    // run getVersion via prepared command
                    builder::detail::ResolvableCommand c2 = *c;
                    auto v = getVersion(s, c2);
                    addProgram(s, PackageId("com.Microsoft.VisualStudio.VC.dumpbin", v), ts, p);
                }
            }

            // libc++
            {
                auto &libcpp = addTarget<PredefinedTarget>(s, PackageId("com.Microsoft.VisualStudio.VC.libcpp", v), ts);
                libcpp.public_ts["system_include_directories"].push_back(normalize_path(idir));
                libcpp.public_ts["system_link_directories"].push_back(normalize_path(root / libdir));

                if (fs::exists(root / "ATLMFC" / "include"))
                {
                    auto &atlmfc = addTarget<PredefinedTarget>(s, PackageId("com.Microsoft.VisualStudio.VC.ATLMFC", v), ts);
                    atlmfc.public_ts["system_include_directories"].push_back(normalize_path(root / "ATLMFC" / "include"));
                    atlmfc.public_ts["system_link_directories"].push_back(normalize_path(root / "ATLMFC" / libdir));
                }
            }
        }
    }
}

static path getProgramFilesX86()
{
    auto e = getenv("programfiles(x86)");
    if (!e)
        throw SW_RUNTIME_ERROR("Cannot get 'programfiles(x86)' env. var.");
    return e;
}

static path getWindowsKitRoot()
{
    // take from registry?
    auto p = getProgramFilesX86() / "Windows Kits";
    if (fs::exists(p))
        return p;
    //throw SW_RUNTIME_ERROR("No Windows Kits available");
    return {};
}

static int getWinRegAccess(DETECT_ARGS)
{
#ifdef _WIN32
    if (s.getHostOs().Version < Version(6, 2))
        return KEY_READ | KEY_WOW64_32KEY;
    return KEY_READ;
#else
    return 0;
#endif
}

static path getWindowsKitRootFromReg(DETECT_ARGS, const std::wstring &root, const std::wstring &key)
{
#ifdef _WIN32
    try
    {
        // may throw if not installed
        winreg::RegKey kits(HKEY_LOCAL_MACHINE, root, getWinRegAccess(s));
        return kits.GetStringValue(L"KitsRoot" + key);
    }
    catch (std::exception &e)
    {
        LOG_TRACE(logger, e.what());
        return {};
    }
#endif
    //throw SW_RUNTIME_ERROR("No Windows Kits available");
    return {};
}

static path getWindows10KitRoot(DETECT_ARGS)
{
    return getWindowsKitRootFromReg(s, L"SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots", L"10");
}

static path getWindows81KitRoot(DETECT_ARGS)
{
    return getWindowsKitRootFromReg(s, L"SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots", L"81");
}

static VersionSet listWindows10Kits(DETECT_ARGS)
{
    VersionSet kits;
#ifdef _WIN32
    try
    {
        winreg::RegKey kits10(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots", getWinRegAccess(s));
        for (auto &k : kits10.EnumSubKeys())
            kits.insert(to_string(k));
    }
    catch (std::exception &)
    {
        // ignore
    }
    // also try directly (kit 10.0.10240 does not register in registry)
    auto kr10 = getWindows10KitRoot(s);
    for (auto &d : fs::directory_iterator(kr10 / "Include"))
    {
        auto k = d.path().filename().string();
        if (fs::exists(kr10 / "Lib" / k) && Version(k).isVersion())
            kits.insert(k);
    }
#endif
    //throw SW_RUNTIME_ERROR("No Windows Kits available");
    return kits;
}

static String getWin10KitDirName()
{
    return "10";
}

static Strings listWindowsKits(DETECT_ARGS)
{
    // https://en.wikipedia.org/wiki/Microsoft_Windows_SDK
    static const Strings known_kits{ "8.1A", "8.1", "8.0", "7.1A", "7.1", "7.0A", "7.0A","6.0A" };

    Strings kits;

    // special handling for win10/81 kits
    auto kr = getWindows10KitRoot(s);
    if (fs::exists(kr))
        kits.push_back(getWin10KitDirName());
    kr = getWindows81KitRoot(s);
    if (fs::exists(kr))
        kits.push_back("8.1");

    kr = getWindowsKitRoot();
    for (auto &k : known_kits)
    {
        auto d = kr / k;
        if (fs::exists(d))
            kits.push_back(k);
    }
    return kits;
}

static void detectWindowsSdk(DETECT_ARGS)
{
    // ucrt - universal CRT
    //
    // um - user mode
    // km - kernel mode
    // shared - some of these and some of these
    //

    auto new_settings = s.getHostOs();

    struct WinKit
    {
        path kit_root;

        String name;

        String bdir_subversion;
        String idir_subversion;
        String ldir_subversion;

        Strings idirs; // additional idirs
        bool without_ldir = false; // when there's not libs

        std::vector<PredefinedTarget*> add(SwCoreContext &s, OS &new_settings, const Version &v)
        {
            auto idir = kit_root / "Include" / idir_subversion;
            if (!fs::exists(idir / name))
            {
                LOG_TRACE(logger, "No include dir " << (idir / name) << " found for library: " << name);
                return {};
            }

            std::vector<PredefinedTarget *> targets;
            for (auto target_arch : { ArchType::x86_64,ArchType::x86,ArchType::arm,ArchType::aarch64 })
            {
                new_settings.Arch = target_arch;

                auto ts1 = toTargetSettings(new_settings);
                TargetSettings ts;
                ts["os"]["kernel"] = ts1["os"]["kernel"];
                ts["os"]["arch"] = ts1["os"]["arch"];

                auto libdir = kit_root / "Lib" / ldir_subversion / name / toStringWindows(target_arch);
                if (fs::exists(libdir))
                {
                    auto &t = addTarget<PredefinedTarget>(s, PackageId("com.Microsoft.Windows.SDK." + name, v), ts);
                    //t.ts["os"]["version"] = v.toString();

                    t.public_ts["system_include_directories"].push_back(normalize_path(idir / name));
                    for (auto &i : idirs)
                        t.public_ts["system_include_directories"].push_back(normalize_path(idir / i));
                    t.public_ts["system_link_directories"].push_back(normalize_path(libdir));
                    targets.push_back(&t);
                }
                else if (without_ldir)
                {
                    auto &t = addTarget<PredefinedTarget>(s, PackageId("com.Microsoft.Windows.SDK." + name, v), ts);
                    //t.ts["os"]["version"] = v.toString();

                    t.public_ts["system_include_directories"].push_back(normalize_path(idir / name));
                    for (auto &i : idirs)
                        t.public_ts["system_include_directories"].push_back(normalize_path(idir / i));
                    targets.push_back(&t);
                }
                else
                    LOG_TRACE(logger, "No libdir " << libdir << " found for library: " << name);
            }
            return targets;
        }

        void addTools(SwCoreContext &s)
        {
            // .rc
            {
                auto p = std::make_shared<SimpleProgram>(s);
                p->file = kit_root / "bin" / bdir_subversion / toStringWindows(s.getHostOs().Arch) / "rc.exe";
                if (fs::exists(p->file))
                {
                    auto v = getVersion(s, p->file, "/?");
                    TargetSettings ts2;
                    auto ts1 = toTargetSettings(s.getHostOs());
                    ts2["os"]["kernel"] = ts1["os"]["kernel"];
                    auto &rc = addProgram(s, PackageId("com.Microsoft.Windows.rc", v), ts2, p);
                }
                // these are passed from compiler during merge?
                //for (auto &idir : COpts.System.IncludeDirectories)
                //C->system_idirs.push_back(idir);
            }

            // .mc
            {
                auto p = std::make_shared<SimpleProgram>(s);
                p->file = kit_root / "bin" / bdir_subversion / toStringWindows(s.getHostOs().Arch) / "mc.exe";
                if (fs::exists(p->file))
                {
                    auto v = getVersion(s, p->file, "/?");
                    auto ts1 = toTargetSettings(s.getHostOs());
                    TargetSettings ts2;
                    ts2["os"]["kernel"] = ts1["os"]["kernel"];
                    auto &rc = addProgram(s, PackageId("com.Microsoft.Windows.mc", v), ts2, p);
                }
                // these are passed from compiler during merge?
                //for (auto &idir : COpts.System.IncludeDirectories)
                //C->system_idirs.push_back(idir);
            }
        }
    };

    for (auto &k : listWindowsKits(s))
    {
        LOG_TRACE(logger, "Found Windows Kit: " + k);

        auto kr = getWindowsKitRoot() / k;
        if (k == getWin10KitDirName())
        {
            for (auto &v : listWindows10Kits(s))
            {
                LOG_TRACE(logger, "Found Windows10 Kit: " + v.toString());

                // win10 kit dir may be different from default kit root,
                // so we update it here
                auto kr10 = getWindows10KitRoot(s);
                if (!kr10.empty())
                    kr = kr10;

                // ucrt
                {
                    WinKit wk;
                    wk.name = "ucrt";
                    wk.kit_root = kr;
                    wk.idir_subversion = v.toString();
                    wk.ldir_subversion = v.toString();
                    wk.add(s, new_settings, v);
                }

                // um + shared
                {
                    WinKit wk;
                    wk.name = "um";
                    wk.kit_root = kr;
                    wk.idir_subversion = v.toString();
                    wk.ldir_subversion = v.toString();
                    wk.idirs.push_back("shared");
                    for (auto t : wk.add(s, new_settings, v))
                        t->public_ts["system_link_libraries"].push_back("kernel32.lib");
                }

                // km
                {
                    WinKit wk;
                    wk.name = "km";
                    wk.kit_root = kr;
                    wk.idir_subversion = v.toString();
                    wk.ldir_subversion = v.toString();
                    wk.add(s, new_settings, v);
                }

                // winrt
                {
                    WinKit wk;
                    wk.name = "winrt";
                    wk.kit_root = kr;
                    wk.idir_subversion = v.toString();
                    wk.without_ldir = true;
                    wk.add(s, new_settings, v);
                }

                // tools
                {
                    WinKit wk;
                    wk.kit_root = kr;
                    wk.bdir_subversion = v.toString();
                    wk.addTools(s);
                }
            }
        }
        else
        {
            // win81 kit dir may be different from default kit root,
            // so we update it here
            if (k == "8.1")
            {
                auto kr81 = getWindows81KitRoot(s);
                if (!kr81.empty())
                    kr = kr81;
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
                wk.add(s, new_settings, k);
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
                wk.add(s, new_settings, k);
            }

            // tools
            {
                WinKit wk;
                wk.kit_root = kr;
                wk.addTools(s);
            }
        }
    }
}

static void detectMsvc(DETECT_ARGS)
{
    detectMsvc15Plus(s);
    detectMsvc14AndOlder(s);
    detectWindowsSdk(s);
}

static bool hasConsoleColorProcessing()
{
#ifdef _WIN32
    bool r = false;
    DWORD mode;
    // Try enabling ANSI escape sequence support on Windows 10 terminals.
    auto console_ = GetStdHandle(STD_OUTPUT_HANDLE);
    if (GetConsoleMode(console_, &mode))
        r |= (bool)(mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    console_ = GetStdHandle(STD_ERROR_HANDLE);
    if (GetConsoleMode(console_, &mode))
        r &= (bool)(mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    return r;
#endif
    return true;
}

static void detectWindowsClang(DETECT_ARGS)
{
    // create programs
    const path base_llvm_path = path("c:") / "Program Files" / "LLVM";
    const path bin_llvm_path = base_llvm_path / "bin";

    bool colored_output = hasConsoleColorProcessing();

    // clang-cl, move to msvc?

    // C, C++
    {
        auto p = std::make_shared<SimpleProgram>(s);
        p->file = bin_llvm_path / "clang-cl.exe";
        //C->file = base_llvm_path / "msbuild-bin" / "cl.exe";
        if (!fs::exists(p->file))
        {
            auto f = resolveExecutable("clang-cl");
            if (fs::exists(f))
                p->file = f;
        }
        if (fs::exists(p->file))
        {
            auto cmd = p->getCommand();
            auto msvc_prefix = detectMsvcPrefix(*cmd, ".");
            getMsvcIncludePrefixes()[p->file] = msvc_prefix;

            auto v = getVersion(s, p->file);
            auto &c = addProgram(s, PackageId("org.LLVM.clangcl", v), {}, p);

            auto c2 = p->getCommand();
            c2->push_back("-X"); // prevents include dirs autodetection
            if (colored_output)
            {
                c2->push_back("-Xclang");
                c2->push_back("-fcolor-diagnostics");
                c2->push_back("-Xclang");
                c2->push_back("-fansi-escape-codes");
            }
        }
    }

    // clang

    // link
    {
        auto p = std::make_shared<SimpleProgram>(s);
        p->file = bin_llvm_path / "lld.exe";
        if (!fs::exists(p->file))
        {
            auto f = resolveExecutable("lld");
            if (fs::exists(f))
                p->file = f;
        }
        if (fs::exists(p->file))
        {
            auto v = getVersion(s, p->file);
            addProgram(s, PackageId("org.LLVM.lld", v), {}, p);

            // this must go into lld-link
            //auto c2 = p->getCommand();
            //c2->push_back("-lldignoreenv"); // prevents libs dirs autodetection (from msvc)
        }
    }

    // ar
    {
        auto p = std::make_shared<SimpleProgram>(s);
        p->file = bin_llvm_path / "llvm-ar.exe";
        if (!fs::exists(p->file))
        {
            auto f = resolveExecutable("llvm-ar");
            if (fs::exists(f))
                p->file = f;
        }
        if (fs::exists(p->file))
        {
            auto v = getVersion(s, p->file);
            addProgram(s, PackageId("org.LLVM.ar", v), {}, p);
        }
    }

    // C
    {
        auto p = std::make_shared<SimpleProgram>(s);
        p->file = bin_llvm_path / "clang.exe";
        if (!fs::exists(p->file))
        {
            auto f = resolveExecutable("clang");
            if (fs::exists(f))
                p->file = f;
        }
        if (fs::exists(p->file))
        {
            auto v = getVersion(s, p->file);
            addProgram(s, PackageId("org.LLVM.clang", v), {}, p);

            if (colored_output)
            {
                auto c2 = p->getCommand();
                c2->push_back("-fcolor-diagnostics");
                c2->push_back("-fansi-escape-codes");
            }
            //c->push_back("-Wno-everything");
            // is it able to find VC STL itself?
            //COpts2.System.IncludeDirectories.insert(base_llvm_path / "lib" / "clang" / C->getVersion().toString() / "include");
        }
    }

    // C++
    {
        auto p = std::make_shared<SimpleProgram>(s);
        p->file = bin_llvm_path / "clang++.exe";
        if (!fs::exists(p->file))
        {
            auto f = resolveExecutable("clang++");
            if (fs::exists(f))
                p->file = f;
        }
        if (fs::exists(p->file))
        {
            auto v = getVersion(s, p->file);
            auto &c = addProgram(s, PackageId("org.LLVM.clangpp", v), {}, p);

            if (colored_output)
            {
                auto c2 = p->getCommand();
                c2->push_back("-fcolor-diagnostics");
                c2->push_back("-fansi-escape-codes");
            }
            //c->push_back("-Wno-everything");
            // is it able to find VC STL itself?
            //COpts2.System.IncludeDirectories.insert(base_llvm_path / "lib" / "clang" / C->getVersion().toString() / "include");
        }
    }
}

static void detectIntelCompilers(DETECT_ARGS)
{
    // some info at https://gitlab.com/ita1024/waf/blob/master/waflib/Tools/msvc.py#L521

    // C, C++

    // win
    {
        auto add_prog_from_path = [&s](const path &name, const String &ppath)
        {
            auto p = std::make_shared<SimpleProgram>(s);
            p->file = resolveExecutable(name);
            if (fs::exists(p->file))
            {
                auto v = getVersion(s, p->file);
                addProgram(s, PackageId(ppath, v), {}, p);

                // icl/xilib/xilink on win wants VC in PATH
                auto &cld = s.getPredefinedTargets();
                auto i = cld["com.Microsoft.VisualStudio.VC.cl"].rbegin_releases();
                if (i != cld["com.Microsoft.VisualStudio.VC.cl"].rend_releases())
                {
                    if (!i->second.empty())
                    {
                        if (auto t = (*i->second.begin())->as<PredefinedProgramTarget *>())
                        {
                            path x = t->getProgram().getCommand()->getProgram();
                            p->getCommand()->addPathDirectory(x.parent_path());
                        }
                    }
                }
            }
            return p;
        };

        add_prog_from_path("icl", "com.intel.compiler.c");
        add_prog_from_path("icl", "com.intel.compiler.cpp");
        add_prog_from_path("xilib", "com.intel.compiler.lib");
        add_prog_from_path("xilink", "com.intel.compiler.link");

        // ICPP_COMPILER{VERSION} like ICPP_COMPILER19 etc.
        for (int i = 9; i < 23; i++)
        {
            auto s = "ICPP_COMPILER" + std::to_string(i);
            auto v = getenv(s.c_str());
            if (!v)
                continue;

            path root = v;
            auto bin = root;
            bin /= "bin";
            auto arch = "intel64";
            bin /= arch;

            std::shared_ptr<SimpleProgram> p;
            p = add_prog_from_path(bin / "icl", "com.intel.compiler.c");
            p->getCommand()->push_back("-I");
            p->getCommand()->push_back(root / "compiler" / "include");

            p = add_prog_from_path(bin / "icl", "com.intel.compiler.cpp");
            p->getCommand()->push_back("-I");
            p->getCommand()->push_back(root / "compiler" / "include");

            add_prog_from_path(bin / "xilib", "com.intel.compiler.lib");

            p = add_prog_from_path(bin / "xilink", "com.intel.compiler.link");
            p->getCommand()->push_back("-LIBPATH:" + (root / "compiler" / "lib" / arch).u8string());
            p->getCommand()->push_back("libirc.lib");
        }

        // also registry paths
        // HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\Intel ...
    }

    // *nix
    {
        {
            auto p = std::make_shared<SimpleProgram>(s); // new object
            p->file = resolveExecutable("icc");
            if (fs::exists(p->file))
            {
                auto v = getVersion(s, p->file);
                addProgram(s, PackageId("com.intel.compiler.c", v), {}, p);
            }
        }

        {
            auto p = std::make_shared<SimpleProgram>(s); // new object
            p->file = resolveExecutable("icpc");
            if (fs::exists(p->file))
            {
                auto v = getVersion(s, p->file);
                addProgram(s, PackageId("com.intel.compiler.cpp", v), {}, p);
            }
        }
    }
}

static void detectWindowsCompilers(DETECT_ARGS)
{
    detectMsvc(s);
    detectWindowsClang(s);
}

static void detectNonWindowsCompilers(DETECT_ARGS)
{
    bool colored_output = hasConsoleColorProcessing();

    auto resolve_and_add = [&s, &colored_output](const path &prog, const String &ppath, int color_diag = 0)
    {
        auto p = std::make_shared<SimpleProgram>(s);
        p->file = resolveExecutable(prog);
        if (fs::exists(p->file))
        {
            // use simple regex for now, because ubuntu may have
            // the following version 7.4.0-1ubuntu1~18.04.1
            // which will be parsed as pre-release
            auto v = getVersion(s, p->file, "--version", "\\d+(\\.\\d+){2,}");
            auto &c = addProgram(s, PackageId(ppath, v), {}, p);
            //-fdiagnostics-color=always // gcc
            if (colored_output)
            {
                auto c2 = p->getCommand();
                if (color_diag == 1)
                    c2->push_back("-fdiagnostics-color=always");
                else if (color_diag == 2)
                {
                    c2->push_back("-fcolor-diagnostics");
                    c2->push_back("-fansi-escape-codes");
                }
            }
        }
    };

    resolve_and_add("ar", "org.gnu.binutils.ar");
    //resolve_and_add("as", "org.gnu.gcc.as"); // not needed
    //resolve_and_add("ld", "org.gnu.gcc.ld"); // not needed

    resolve_and_add("gcc", "org.gnu.gcc", 1);
    resolve_and_add("g++", "org.gnu.gpp", 1);

    for (int i = 3; i < 12; i++)
    {
        resolve_and_add("gcc-" + std::to_string(i), "org.gnu.gcc", 1);
        resolve_and_add("g++-" + std::to_string(i), "org.gnu.gpp", 1);
    }

    // llvm/clang
    //resolve_and_add("llvm-ar", "org.LLVM.ar"); // not needed
    //resolve_and_add("lld", "org.LLVM.ld"); // not needed

    resolve_and_add("clang", "org.LLVM.clang", 2);
    resolve_and_add("clang++", "org.LLVM.clangpp", 2);

    for (int i = 3; i < 16; i++)
    {
        resolve_and_add("clang-" + std::to_string(i), "org.LLVM.clang", 2);
        resolve_and_add("clang++-" + std::to_string(i), "org.LLVM.clangpp", 2);
    }

    // detect apple clang?
}

void detectNativeCompilers(DETECT_ARGS)
{
    auto &os = s.getHostOs();
    if (os.is(OSType::Windows) || os.is(OSType::Cygwin))
    {
        if (os.is(OSType::Cygwin))
            detectNonWindowsCompilers(s);
        detectWindowsCompilers(s);
    }
    else
        detectNonWindowsCompilers(s);
    detectIntelCompilers(s);
}

void setHostPrograms(const SwCoreContext &swctx, TargetSettings &ts, bool force)
{
    auto to_upkg = [](const auto &s)
    {
        return UnresolvedPackage(s).toString();
    };

    auto check_and_assign = [force](auto &k, const auto &v, bool force2 = false)
    {
        if (force || !k || force2)
            k = v;
    };

    // settings
#ifdef _WIN32
#ifdef NDEBUG
    check_and_assign(ts["native"]["configuration"], "release");
#else
    check_and_assign(ts["native"]["configuration"], "debug");
#endif
#else
    check_and_assign(ts["native"]["configuration"], "release");
#endif
    check_and_assign(ts["native"]["library"], "shared");
    check_and_assign(ts["native"]["mt"], "false");

    // deps: programs, stdlib etc.
    auto check_and_assign_dependency = [&check_and_assign, &swctx, &ts, force](auto &k, const auto &v, int version_level = 0)
    {
        bool use_k = !force && k && k.isValue();
        auto i = swctx.getPredefinedTargets().find(UnresolvedPackage(use_k ? k.getValue() : v), ts);
        if (i)
            check_and_assign(k, version_level ? i->getPackage().toString(version_level) : i->getPackage().toString(), use_k);
        else
            check_and_assign(k, v);
    };

    if (swctx.getHostOs().is(OSType::Windows))
    {
        check_and_assign_dependency(ts["native"]["stdlib"]["c"], to_upkg("com.Microsoft.Windows.SDK.ucrt"));
        check_and_assign_dependency(ts["native"]["stdlib"]["cpp"], to_upkg("com.Microsoft.VisualStudio.VC.libcpp"));
        check_and_assign_dependency(ts["native"]["stdlib"]["kernel"], to_upkg("com.Microsoft.Windows.SDK.um"));

        // now find the latest available sdk (ucrt) and select it
        //TargetSettings oss;
        //oss["os"] = ts["os"];
        //auto sdk = swctx.getPredefinedTargets().find(UnresolvedPackage(ts["native"]["stdlib"]["c"].getValue()), oss);
        //if (!sdk)
            //throw SW_RUNTIME_ERROR("No suitable installed WinSDK found for this host");
        //ts["native"]["stdlib"]["c"] = sdk->getPackage().toString(); // assign always
        //ts["os"]["version"] = sdkver->toString(3); // cut off the last (fourth) number

        auto clpkg = "com.Microsoft.VisualStudio.VC.cl";
        auto cl = swctx.getPredefinedTargets().find(clpkg);

        auto clangpppkg = "org.LLVM.clangpp";
        auto clangpp = swctx.getPredefinedTargets().find(clpkg);

        if (0);
#ifdef _MSC_VER
        // msvc + clangcl
        // clangcl must be compatible with msvc
        // and also clang actually
        else if (cl != swctx.getPredefinedTargets().end(clpkg) && !cl->second.empty())
        {
            check_and_assign_dependency(ts["native"]["program"]["c"], to_upkg("com.Microsoft.VisualStudio.VC.cl"));
            check_and_assign_dependency(ts["native"]["program"]["cpp"], to_upkg("com.Microsoft.VisualStudio.VC.cl"));
            check_and_assign_dependency(ts["native"]["program"]["asm"], to_upkg("com.Microsoft.VisualStudio.VC.ml"));
            check_and_assign_dependency(ts["native"]["program"]["lib"], to_upkg("com.Microsoft.VisualStudio.VC.lib"));
            check_and_assign_dependency(ts["native"]["program"]["link"], to_upkg("com.Microsoft.VisualStudio.VC.link"));
        }
        // separate?
#else __clang__
        else if (clangpp != swctx.getPredefinedTargets().end(clangpppkg) && !clangpp->second.empty())
        {
            check_and_assign_dependency(ts["native"]["program"]["c"], to_upkg("org.LLVM.clang"));
            check_and_assign_dependency(ts["native"]["program"]["cpp"], to_upkg("org.LLVM.clangpp"));
            check_and_assign_dependency(ts["native"]["program"]["asm"], to_upkg("org.LLVM.clang"));
            // ?
            check_and_assign_dependency(ts["native"]["program"]["lib"], to_upkg("com.Microsoft.VisualStudio.VC.lib"));
            check_and_assign_dependency(ts["native"]["program"]["link"], to_upkg("com.Microsoft.VisualStudio.VC.link"));
        }
#endif
        // add more defaults (clangcl, clang)
        else
            throw SW_RUNTIME_ERROR("Seems like you do not have Visual Studio installed.\nPlease, install the latest Visual Studio first.");
    }
    // add more defaults
    else
    {
        // set default libs?
        /*ts["native"]["stdlib"]["c"] = to_upkg("com.Microsoft.Windows.SDK.ucrt");
        ts["native"]["stdlib"]["cpp"] = to_upkg("com.Microsoft.VisualStudio.VC.libcpp");
        ts["native"]["stdlib"]["kernel"] = to_upkg("com.Microsoft.Windows.SDK.um");*/

        auto if_add = [&swctx, &check_and_assign_dependency](auto &s, const UnresolvedPackage &name)
        {
            auto &pd = swctx.getPredefinedTargets();
            auto i = pd.find(name);
            if (i == pd.end() || i->second.empty())
                return false;
            check_and_assign_dependency(s, name.toString());
            return true;
        };

        auto err_msg = [](const String &cl)
        {
            return "sw was built with " + cl + " as compiler, but it was not found in your system. Install " + cl + " to proceed.";
        };

        // must be the same compiler as current!
#if defined(__clang__)
        if (!(
            if_add(ts["native"]["program"]["c"], "org.LLVM.clang"s) &&
            if_add(ts["native"]["program"]["cpp"], "org.LLVM.clangpp"s)
            ))
        {
            throw SW_RUNTIME_ERROR(err_msg("clang"));
        }
        //if (getHostOs().is(OSType::Linux))
        //ts["native"]["stdlib"]["cpp"] = to_upkg("org.sw.demo.llvm_project.libcxx");
#elif defined(__GNUC__)
        if (!(
            if_add(ts["native"]["program"]["c"], "org.gnu.gcc") &&
            if_add(ts["native"]["program"]["cpp"], "org.gnu.gpp")
            ))
        {
            throw SW_RUNTIME_ERROR(err_msg("gcc"));
        }
#elif !defined(_WIN32)
#error "Add your current compiler to detect.cpp and here."
#endif

        // using c prog
        if_add(ts["native"]["program"]["asm"], ts["native"]["program"]["c"].getValue());

        // reconsider, also with driver?
        if_add(ts["native"]["program"]["lib"], "org.gnu.binutils.ar"s);

        // use driver
        // use cpp driver for the moment to not burden ourselves in adding stdlib
        if_add(ts["native"]["program"]["link"], ts["native"]["program"]["cpp"].getValue());
    }
}

}
