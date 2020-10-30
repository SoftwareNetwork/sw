// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include "detect.h"

#include "../misc/cmVSSetupHelper.h"
#include "../build.h"
#include "../command.h"
#include "../program_version_storage.h"

#include <boost/algorithm/string.hpp>
#include <primitives/command.h>

#include <regex>
#include <string>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "compiler.detect");

// TODO: actually detect.cpp may be rewritten as entry point

namespace sw
{

ProgramDetector &getProgramDetector()
{
    static ProgramDetector pd;
    return pd;
}

static String detectMsvcPrefix(builder::detail::ResolvableCommand c)
{
    // examples:
    // "Note: including file: filename\r" (english)
    // "Some: other lang: filename\r"
    // "Some: other lang  filename\r" (ita)

    auto basefn = support::get_temp_filename("cliprefix");
    auto fn = path(basefn) += ".c";
    auto hfn = path(basefn) += ".h";
    String contents = "#include \"" + to_string(normalize_path(hfn)) + "\"\r\nint dummy;";
    auto obj = path(fn) += ".obj";
    write_file(fn, contents);
    write_file(hfn, "");
    c.push_back("/showIncludes");
    c.push_back("/c");
    c.push_back(fn);
    c.push_back("/Fo" + to_string(normalize_path_windows(obj)));
    std::error_code ec;
    c.execute(ec);
    fs::remove(obj);
    fs::remove(fn);
    fs::remove(hfn);

    auto error = [&c](const String &reason)
    {
        return "Cannot match VS include prefix (" + reason + "):\n" + c.out.text + "\nstderr:\n" + c.err.text;
    };

    auto lines = split_lines(c.out.text);
    if (lines.empty())
        throw SW_RUNTIME_ERROR(error("bad output"));

    String s = R"((.*?\s)[a-zA-Z]:[\\\/].*)" + hfn.stem().string() + "\\" + hfn.extension().string();
    std::regex r(s);
    std::smatch m;
    // clang-cl does not output filename -> lines.size() == 1
    if (!std::regex_search(lines.size() > 1 ? lines[1] : lines[0], m, r))
        throw SW_RUNTIME_ERROR(error("regex_search failed"));
    return m[1].str();
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

ProgramDetector::ProgramDetector()
{
}

String ProgramDetector::getMsvcPrefix(builder::detail::ResolvableCommand c)
{
    auto &p = getMsvcIncludePrefixes();
    if (!p[c.getProgram()].empty())
        return p[c.getProgram()];
    return p[c.getProgram()] = detectMsvcPrefix(c);
}

String ProgramDetector::getMsvcPrefix(const path &prog) const
{
    auto &p = getMsvcIncludePrefixes();
    auto i = p.find(prog);
    if (i == p.end())
        throw SW_RUNTIME_ERROR("Cannot find msvc prefix");
    return i->second;
}

vs::RuntimeLibraryType ProgramDetector::getMsvcLibraryType(const BuildSettings &bs)
{
    auto rt = vs::RuntimeLibraryType::MultiThreadedDLL;
    if (bs.Native.MT)
        rt = vs::RuntimeLibraryType::MultiThreaded;
    if (bs.Native.ConfigurationType == ConfigurationType::Debug)
    {
        rt = vs::RuntimeLibraryType::MultiThreadedDLLDebug;
        if (bs.Native.MT)
            rt = vs::RuntimeLibraryType::MultiThreadedDebug;
    }
    return rt;
}

String ProgramDetector::getMsvcLibraryName(const String &base, const BuildSettings &bs)
{
    switch (getMsvcLibraryType(bs))
    {
    case vs::RuntimeLibraryType::MultiThreadedDLL:
        return base + ".lib";
    case vs::RuntimeLibraryType::MultiThreadedDLLDebug:
        return base + "d.lib";
    case vs::RuntimeLibraryType::MultiThreaded:
        return "lib" + base + ".lib";
    case vs::RuntimeLibraryType::MultiThreadedDebug:
        return "lib" + base + "d.lib";
    default:
        SW_UNIMPLEMENTED;
    }
}

ProgramDetector::DetectablePackageEntryPoints ProgramDetector::getDetectablePackages()
{
    DetectablePackageEntryPoints s;
    auto add_eps = [&s](auto &eps)
    {
        using Map = std::unordered_map<UnresolvedPackage, std::vector<DetectablePackageEntryPoint>>;
        Map m;
        for (auto &[u, f] : eps)
            m[u].push_back(std::move(f));
        for (auto &[u, v] : m)
        {
            s[u] = [v = std::move(v)](DETECT_ARGS)
            {
                for (auto &f : v)
                    f(DETECT_ARGS_PASS);
            };
        }
    };
    add_eps(getProgramDetector().detectMsvc());
    return s;
}

void ProgramDetector::log_msg_detect_target(const String &m)
{
    //LOG_TRACE(logger, m);
}

PredefinedProgramTarget &ProgramDetector::addProgram(DETECT_ARGS, const PackageId &id, const TargetSettings &ts, const Program &p)
{
    auto &t = addTarget<PredefinedProgramTarget>(DETECT_ARGS_PASS, id, ts);
    t.public_ts["output_file"] = to_string(normalize_path(p.file));
    t.setProgram(p.clone());
    log_msg_detect_target("Detected program: " + to_string(p.file.u8string()));
    return t;
}

ProgramDetector::VSInstances ProgramDetector::gatherVSInstances()
{
    VSInstances instances;
#ifdef _WIN32
    CoInitializeEx(0, 0);

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

    CoUninitialize();
#endif
    return instances;
}

ProgramDetector::VSInstances &ProgramDetector::getVSInstances() const
{
    if (vsinstances1.empty())
        vsinstances1 = gatherVSInstances();
    return vsinstances1;
}

ProgramDetector::MsvcInstance::MsvcInstance(const VSInstance &i)
    : i(i)
{
    bool vs15plus = i.version.getMajor() >= 15;

    root = i.root / "VC";
    if (vs15plus)
        root = root / "Tools" / "MSVC" / boost::trim_copy(read_file(root / "Auxiliary" / "Build" / "Microsoft.VCToolsVersion.default.txt"));
    compiler = root / "bin";
    idir = root / "include";
}

void ProgramDetector::MsvcInstance::process(DETECT_ARGS)
{
    bool vs15plus = i.version.getMajor() >= 15;
    auto &eb = static_cast<ExtendedBuild &>(b);
    BuildSettings new_settings = eb.getSettings();
    const auto host = toStringWindows(b.getContext().getHostOs().Arch);

    // get suffix
    target_arch = new_settings.TargetOS.Arch;
    if (vs15plus)
    {
        target = toStringWindows(target_arch);
        compiler /= "Host" + host;
        host_root = compiler / host;
        compiler /= target;
    }
    else
    {
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
        target = toStringWindows14AndOlder(target_arch);
        host_root = compiler;

        // VC/bin/ ... x86 files
        // VC/bin/amd64/ ... x86_64 files
        // VC/bin/arm/ ... arm files
        // so we need to add subdir for non x86 targets
        if (!b.getContext().getHostOs().is(ArchType::x86))
        {
            host_root /= toStringWindows14AndOlder(b.getContext().getHostOs().Arch);
        }

        // now set to root
        compiler = host_root;

        // VC/bin/x86_amd64
        // VC/bin/x86_arm
        // VC/bin/amd64_x86
        // VC/bin/amd64_arm
        if (b.getContext().getHostOs().Arch != target_arch)
        {
            //if (!s.getHostOs().is(ArchType::x86))
            compiler += "_"s + toStringWindows14AndOlder(target_arch);
        }
    }

    // now get real cl version
    auto p = std::make_shared<SimpleProgram>();
    p->file = compiler / "cl.exe";
    if (!fs::exists(p->file))
        return;

    auto c = p->getCommand();
    if (b.getContext().getHostOs().Arch != target_arch)
        c->addPathDirectory(host_root);
    msvc_prefix = getProgramDetector().getMsvcPrefix(*c);
    // run getVersion via prepared command
    builder::detail::ResolvableCommand c2 = *c;
    cl_exe_version = getVersion(b.getContext(), c2);
    if (i.version.isPreRelease())
        cl_exe_version.getExtra() = i.version.getExtra();
}

ProgramDetector::DetectablePackageMultiEntryPoints ProgramDetector::detectMsvcCommon(const MsvcInstance &m)
{
    DetectablePackageMultiEntryPoints eps;

    // VS programs inherit cl.exe version (V)
    // same for VS libs
    // because ml,ml64,lib,link version (O) has O.Major = V.Major - 5
    // e.g., V = 19.21..., O = 14.21.... (19 - 5 = 14)

    //String msvc_prefix;
    //Version cl_exe_version;

    // C, C++
    eps.emplace("com.Microsoft.VisualStudio.VC.cl", [this, m = m](DETECT_ARGS) mutable
    {
        auto &eb = static_cast<ExtendedBuild &>(b);
        m.process(DETECT_ARGS_PASS);

        auto p = std::make_unique<VisualStudioCompiler>();
        p->file = m.compiler / "cl.exe";
        if (!fs::exists(p->file))
            return;

        //auto c = p->getCommand();
        if (b.getContext().getHostOs().Arch != m.target_arch)
        {
            SW_UNIMPLEMENTED;
            //c->addPathDirectory(m.host_root);
        }
        auto &cl = addProgram(DETECT_ARGS_PASS, PackageId("com.Microsoft.VisualStudio.VC.cl", m.cl_exe_version), eb.getSettings(), *p);
        auto r = std::make_unique<NativeCompilerRule>(p->clone());
        r->lang = NativeCompilerRule::LANG_C;
        cl.setRule("c", std::move(r));
        r = std::make_unique<NativeCompilerRule>(p->clone());
        r->lang = NativeCompilerRule::LANG_CPP;
        cl.setRule("cpp", std::move(r));
        //r->rulename = "[C++]"; // CXX?
        //r->rulename = "[C]";
    });

    // lib, link
    eps.emplace("com.Microsoft.VisualStudio.VC.link", [this, m = m](DETECT_ARGS) mutable
    {
        auto &eb = static_cast<ExtendedBuild &>(b);
        m.process(DETECT_ARGS_PASS);

        auto p = std::make_unique<VisualStudioLinker>();
        p->file = m.compiler / "link.exe";
        if (!fs::exists(p->file))
            return;

        if (b.getContext().getHostOs().Arch != m.target_arch)
        {
            auto c = p->getCommand();
            c->addPathDirectory(m.host_root);
        }
        auto &t = addProgram(DETECT_ARGS_PASS, PackageId("com.Microsoft.VisualStudio.VC.link", m.cl_exe_version), eb.getSettings(), *p);
        t.setRule("link", std::make_unique<NativeLinkerRule>(std::move(p)));
    });

    eps.emplace("com.Microsoft.VisualStudio.VC.lib", [this, m = m](DETECT_ARGS) mutable
    {
        auto &eb = static_cast<ExtendedBuild &>(b);
        m.process(DETECT_ARGS_PASS);

        auto p = std::make_unique<VisualStudioLibrarian>();
        p->file = m.compiler / "lib.exe";
        if (!fs::exists(p->file))
            return;
        if (b.getContext().getHostOs().Arch != m.target_arch)
        {
            auto c = p->getCommand();
            c->addPathDirectory(m.host_root);
        }
        auto &t = addProgram(DETECT_ARGS_PASS, PackageId("com.Microsoft.VisualStudio.VC.lib", m.cl_exe_version), eb.getSettings(), *p);
        auto r = std::make_unique<NativeLinkerRule>(std::move(p));
        r->is_linker = false;
        t.setRule("lib", std::move(r));
    });

    // ASM
    eps.emplace("com.Microsoft.VisualStudio.VC.ml", [this, m = m](DETECT_ARGS) mutable
    {
        auto &eb = static_cast<ExtendedBuild &>(b);
        m.process(DETECT_ARGS_PASS);

        if (m.target_arch == ArchType::x86_64 || m.target_arch == ArchType::x86)
        {
            auto p = std::make_shared<VisualStudioASMCompiler>();
            p->file = m.compiler / (m.target_arch == ArchType::x86_64 ? "ml64.exe" : "ml.exe");
            if (!fs::exists(p->file))
                return;
            getMsvcIncludePrefixes()[p->file] = m.msvc_prefix;
            auto &t = addProgram(DETECT_ARGS_PASS, PackageId("com.Microsoft.VisualStudio.VC.ml", m.cl_exe_version), eb.getSettings(), *p);
            auto r = std::make_unique<NativeCompilerRule>(p->clone());
            r->lang = NativeCompilerRule::LANG_ASM;
            t.setRule("asm", std::move(r));
            //r->rulename = "[ASM]";
        }
    });

    // dumpbin
    /*{
        auto p = std::make_shared<SimpleProgram>();
        p->file = compiler / "dumpbin.exe";
        if (fs::exists(p->file))
            addProgram(DETECT_ARGS_PASS, PackageId("com.Microsoft.VisualStudio.VC.dumpbin", cl_exe_version), ts, p);
        // should we add path dir here?
    }*/

    // msvc libs
    // https://docs.microsoft.com/en-us/cpp/c-runtime-library/crt-library-features?view=vs-2019

    // TODO: libs may have further versions like
    // libcpmt.lib
    // libcpmt1.lib
    //
    // libcpmtd.lib
    // libcpmtd0.lib
    // libcpmtd1.lib
    //
    // libconcrt.lib
    // libconcrt1.lib
    //
    // libconcrtd.lib
    // libconcrtd0.lib
    // libconcrtd1.lib

    // libc++
    eps.emplace("com.Microsoft.VisualStudio.VC.libcpp", [this, m = m](DETECT_ARGS) mutable
    {
        auto &eb = static_cast<ExtendedBuild &>(b);
        BuildSettings new_settings = eb.getSettings();
        m.process(DETECT_ARGS_PASS);

        auto &libcpp = addTarget<PredefinedTarget>(DETECT_ARGS_PASS, PackageId("com.Microsoft.VisualStudio.VC.libcpp", m.cl_exe_version), eb.getSettings());
        libcpp.public_ts["properties"]["6"]["system_include_directories"].push_back(m.idir);
        auto no_target_libdir = m.i.version.getMajor() < 16 && m.target == "x86";
        if (no_target_libdir)
            libcpp.public_ts["properties"]["6"]["system_link_directories"].push_back(m.root / "lib");
        else
            libcpp.public_ts["properties"]["6"]["system_link_directories"].push_back(m.root / "lib" / m.target);
        if (m.cl_exe_version.getMajor() >= 19)
        {
            // we also add some other libs needed by msvc
            // oldnames.lib - for backward compat - https://docs.microsoft.com/en-us/cpp/c-runtime-library/backward-compatibility?view=vs-2019

            // under cond?
            libcpp.public_ts["properties"]["6"]["system_link_libraries"].push_back(boost::to_upper_copy("oldnames.lib"s));

            // 100% under cond
            libcpp.public_ts["properties"]["6"]["system_link_libraries"].push_back(boost::to_upper_copy("legacy_stdio_definitions.lib"s));
            libcpp.public_ts["properties"]["6"]["system_link_libraries"].push_back(boost::to_upper_copy("legacy_stdio_wide_specifiers.lib"s));
        }
        switch (sw::getProgramDetector().getMsvcLibraryType(new_settings))
        {
        case vs::RuntimeLibraryType::MultiThreadedDLL:
            libcpp.public_ts["properties"]["6"]["system_link_libraries"].push_back(boost::to_upper_copy("msvcprt.lib"s));
            break;
        case vs::RuntimeLibraryType::MultiThreadedDLLDebug:
            libcpp.public_ts["properties"]["6"]["system_link_libraries"].push_back(boost::to_upper_copy("msvcprtd.lib"s));
            break;
        case vs::RuntimeLibraryType::MultiThreaded:
            libcpp.public_ts["properties"]["6"]["system_link_libraries"].push_back(boost::to_upper_copy("libcpmt.lib"s));
            break;
        case vs::RuntimeLibraryType::MultiThreadedDebug:
            libcpp.public_ts["properties"]["6"]["system_link_libraries"].push_back(boost::to_upper_copy("libcpmtd.lib"s));
            break;
        default:
            SW_UNIMPLEMENTED;
        }
        // add to separate lib?
        switch (sw::getProgramDetector().getMsvcLibraryType(new_settings))
        {
        case vs::RuntimeLibraryType::MultiThreadedDLL:
            libcpp.public_ts["properties"]["6"]["system_link_libraries"].push_back(boost::to_upper_copy("msvcrt.lib"s));
            break;
        case vs::RuntimeLibraryType::MultiThreadedDLLDebug:
            libcpp.public_ts["properties"]["6"]["system_link_libraries"].push_back(boost::to_upper_copy("msvcrtd.lib"s));
            break;
        case vs::RuntimeLibraryType::MultiThreaded:
            libcpp.public_ts["properties"]["6"]["system_link_libraries"].push_back(boost::to_upper_copy("libcmt.lib"s));
            break;
        case vs::RuntimeLibraryType::MultiThreadedDebug:
            libcpp.public_ts["properties"]["6"]["system_link_libraries"].push_back(boost::to_upper_copy("libcmtd.lib"s));
            break;
        default:
            SW_UNIMPLEMENTED;
        }
    });

    eps.emplace("com.Microsoft.VisualStudio.VC.ATLMFC", [this, m = m](DETECT_ARGS) mutable
    {
        if (!fs::exists(m.root / "ATLMFC" / "include"))
            return;

        auto &eb = static_cast<ExtendedBuild &>(b);
        m.process(DETECT_ARGS_PASS);
        auto no_target_libdir = m.i.version.getMajor() < 16 && m.target == "x86";

        auto &atlmfc = addTarget<PredefinedTarget>(DETECT_ARGS_PASS, PackageId("com.Microsoft.VisualStudio.VC.ATLMFC", m.cl_exe_version), eb.getSettings());
        atlmfc.public_ts["properties"]["6"]["system_include_directories"].push_back(m.root / "ATLMFC" / "include");
        if (no_target_libdir)
            atlmfc.public_ts["properties"]["6"]["system_link_directories"].push_back(m.root / "ATLMFC" / "lib");
        else
            atlmfc.public_ts["properties"]["6"]["system_link_directories"].push_back(m.root / "ATLMFC" / "lib" / m.target);
    });

    // concrt
    eps.emplace("com.Microsoft.VisualStudio.VC.concrt", [this, m = m](DETECT_ARGS) mutable
    {
        if (!fs::exists(m.root / "crt" / "src" / "concrt"))
            return;

        auto &eb = static_cast<ExtendedBuild &>(b);
        BuildSettings new_settings = eb.getSettings();
        m.process(DETECT_ARGS_PASS);

        if (m.cl_exe_version.getMajor() < 19)
            return;

        auto &concrt = addTarget<PredefinedTarget>(DETECT_ARGS_PASS, PackageId("com.Microsoft.VisualStudio.VC.concrt", m.cl_exe_version), eb.getSettings());
        // protected?
        concrt.public_ts["properties"]["6"]["system_include_directories"].push_back(m.root / "crt" / "src" / "concrt");
        concrt.public_ts["properties"]["6"]["system_link_libraries"].push_back(
            boost::to_upper_copy(sw::getProgramDetector().getMsvcLibraryName("concrt", new_settings)));
    });

    // vcruntime
    eps.emplace("com.Microsoft.VisualStudio.VC.runtime", [this, m = m](DETECT_ARGS) mutable
    {
        if (!fs::exists(m.root / "crt" / "src" / "vcruntime"))
            return;

        auto &eb = static_cast<ExtendedBuild &>(b);
        BuildSettings new_settings = eb.getSettings();
        m.process(DETECT_ARGS_PASS);

        if (m.cl_exe_version.getMajor() < 19)
            return;

        auto &vcruntime = addTarget<PredefinedTarget>(DETECT_ARGS_PASS, PackageId("com.Microsoft.VisualStudio.VC.runtime", m.cl_exe_version), eb.getSettings());
        // protected?
        //vcruntime.public_ts["properties"]["6"]["system_include_directories"].push_back(root / "crt" / "src" / "vcruntime");
        vcruntime.public_ts["properties"]["6"]["system_link_libraries"].push_back(
            boost::to_upper_copy(sw::getProgramDetector().getMsvcLibraryName("vcruntime", new_settings)));
    });

    return eps;
}

ProgramDetector::DetectablePackageMultiEntryPoints ProgramDetector::detectMsvc15Plus()
{
    // https://docs.microsoft.com/en-us/cpp/c-runtime-library/crt-library-features?view=vs-2019

    DetectablePackageMultiEntryPoints eps;
    for (auto &[_, instance] : getVSInstances())
        eps.merge(detectMsvcCommon(instance));
    return eps;
}

ProgramDetector::DetectablePackageMultiEntryPoints ProgramDetector::detectMsvc14AndOlder()
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

    // no ArchType::aarch64?

    // following code is written using VS2015
    // older versions might need special handling

    DetectablePackageMultiEntryPoints eps;
    for (auto n : {14,12,11,10,9,8})
    {
        Version v(n);
        auto root = find_comn_tools(v);
        if (root.empty())
            continue;

        VSInstance i;
        i.root = root;
        i.version = v;
        eps.merge(detectMsvcCommon(i));
    }
    return eps;
}

ProgramDetector::DetectablePackageMultiEntryPoints ProgramDetector::detectMsvc()
{
    DetectablePackageMultiEntryPoints eps;
    eps.merge(detectMsvc15Plus());
    eps.merge(detectMsvc14AndOlder());
    eps.merge(detectWindowsSdk());
    return eps;
}

void ProgramDetector::detectWindowsClang(DETECT_ARGS)
{
    // create programs
    const path base_llvm_path = path("c:") / "Program Files" / "LLVM";
    const path bin_llvm_path = base_llvm_path / "bin";

    bool colored_output = hasConsoleColorProcessing();

    // clang-cl, move to msvc?

    // C, C++
    /*{
        auto p = std::make_shared<SimpleProgram>();
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
            auto msvc_prefix = getMsvcPrefix(*cmd);
            getMsvcIncludePrefixes()[p->file] = msvc_prefix;

            auto [o,v] = getVersionAndOutput(s, p->file);

            // check before adding target
            static std::regex r("InstalledDir: (.*)\\r?\\n?");
            std::smatch m;
            if (std::regex_search(o, m, r))
            {
                auto &c = addProgram(DETECT_ARGS_PASS, PackageId("org.LLVM.clangcl", v), {}, p);

                auto c2 = p->getCommand();
                //c2->push_back("-X"); // prevents include dirs autodetection
                // we use --nostdinc, so -X is not needed
                if (colored_output)
                {
                    c2->push_back("-Xclang");
                    c2->push_back("-fcolor-diagnostics");
                    c2->push_back("-Xclang");
                    c2->push_back("-fansi-escape-codes");
                }

                // returns path to /bin dir
                path dir = m[1].str();
                dir = dir.parent_path() / "lib/clang" / v.toString() / "include";
                auto s = to_string(normalize_path(dir));
                auto arg = std::make_unique<primitives::command::SimplePositionalArgument>("-I" + s);
                arg->getPosition().push_back(150);
                c2->push_back(std::move(arg));
            }
            else
            {
                LOG_ERROR(logger, "Cannot get clang-cl installed dir (InstalledDir): " + o);
            }
        }
    }

    // clang

    // link
    {
        auto p = std::make_shared<SimpleProgram>();
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
            addProgram(DETECT_ARGS_PASS, PackageId("org.LLVM.lld", v), {}, p);
        }
    }

    // lld-link
    {
        auto p = std::make_shared<SimpleProgram>();
        p->file = bin_llvm_path / "lld-link.exe";
        if (!fs::exists(p->file))
        {
            auto f = resolveExecutable("lld-link");
            if (fs::exists(f))
                p->file = f;
        }
        if (fs::exists(p->file))
        {
            auto v = getVersion(s, p->file);
            addProgram(DETECT_ARGS_PASS, PackageId("org.LLVM.lld.link", v), {}, p);

            auto c2 = p->getCommand();
            c2->push_back("-lldignoreenv"); // prevents libs dirs autodetection (from msvc)
        }
    }

    // ar
    {
        auto p = std::make_shared<SimpleProgram>();
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
            addProgram(DETECT_ARGS_PASS, PackageId("org.LLVM.ar", v), {}, p);
        }
    }

    // C
    {
        auto p = std::make_shared<SimpleProgram>();
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
            addProgram(DETECT_ARGS_PASS, PackageId("org.LLVM.clang", v), {}, p);

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
        auto p = std::make_shared<SimpleProgram>();
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
            auto &c = addProgram(DETECT_ARGS_PASS, PackageId("org.LLVM.clangpp", v), {}, p);

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
    }*/
}

void ProgramDetector::detectIntelCompilers(DETECT_ARGS)
{
    // some info at https://gitlab.com/ita1024/waf/blob/master/waflib/Tools/msvc.py#L521

    // C, C++

    SW_UNIMPLEMENTED;

    // win
    /*{
        auto add_prog_from_path = [DETECT_ARGS_PASS_TO_LAMBDA](const path &name, const String &ppath)
        {
            auto p = std::make_shared<SimpleProgram>();
            p->file = resolveExecutable(name);
            if (fs::exists(p->file))
            {
                auto v = getVersion(s, p->file);
                addProgram(DETECT_ARGS_PASS, PackageId(ppath, v), {}, p);

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
            p->getCommand()->push_back("-LIBPATH:" + to_string((root / "compiler" / "lib" / arch).u8string()));
            p->getCommand()->push_back("libirc.lib");
        }

        // also registry paths
        // HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\Intel ...
    }

    // *nix
    {
        {
            auto p = std::make_shared<SimpleProgram>(); // new object
            p->file = resolveExecutable("icc");
            if (fs::exists(p->file))
            {
                auto v = getVersion(s, p->file);
                addProgram(DETECT_ARGS_PASS, PackageId("com.intel.compiler.c", v), {}, p);
            }
        }

        {
            auto p = std::make_shared<SimpleProgram>(); // new object
            p->file = resolveExecutable("icpc");
            if (fs::exists(p->file))
            {
                auto v = getVersion(s, p->file);
                addProgram(DETECT_ARGS_PASS, PackageId("com.intel.compiler.cpp", v), {}, p);
            }
        }
    }*/
}

void ProgramDetector::detectWindowsCompilers(DETECT_ARGS)
{
    SW_UNIMPLEMENTED;
    //detectMsvc(DETECT_ARGS_PASS);
    detectWindowsClang(DETECT_ARGS_PASS);
}

void ProgramDetector::detectNonWindowsCompilers(DETECT_ARGS)
{
    bool colored_output = hasConsoleColorProcessing();

    SW_UNIMPLEMENTED;
    /*auto resolve_and_add = [DETECT_ARGS_PASS_TO_LAMBDA, &colored_output]
    (const path &prog, const String &ppath, int color_diag = 0, const String &regex_prefix = {})
    {
        auto p = std::make_shared<SimpleProgram>();
        p->file = resolveExecutable(prog);
        if (!fs::exists(p->file))
            return false;
        // use simple regex for now, because ubuntu may have
        // the following version 7.4.0-1ubuntu1~18.04.1
        // which will be parsed as pre-release
        auto v = getVersion(s, p->file, "--version", regex_prefix + "\\d+(\\.\\d+){2,}");
        auto &c = addProgram(DETECT_ARGS_PASS, PackageId(ppath, v), {}, p);
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
        return true;
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

    // start of the line (^) does not work currently,
    // so we can't differentiate clang and appleclang
    auto clang_regex_prefix = "^clang version ";
    auto apple_clang_regex_prefix = "Apple clang version ";

    // detect apple clang
    bool apple_clang_found =
        resolve_and_add("clang", "com.Apple.clang", 2, apple_clang_regex_prefix);
    resolve_and_add("clang++", "com.Apple.clangpp", 2, apple_clang_regex_prefix);

    // usual clang
    // if apple clang is found first, we do not check same binaries again
    // because at the moment we gen false positives
    if (!apple_clang_found)
    {
        resolve_and_add("clang", "org.LLVM.clang", 2, clang_regex_prefix);
        resolve_and_add("clang++", "org.LLVM.clangpp", 2, clang_regex_prefix);
    }

    for (int i = 3; i < 16; i++)
    {
        resolve_and_add("clang-" + std::to_string(i), "org.LLVM.clang", 2, clang_regex_prefix);
        resolve_and_add("clang++-" + std::to_string(i), "org.LLVM.clangpp", 2, clang_regex_prefix);
    }*/
}

void ProgramDetector::detectNativeCompilers(DETECT_ARGS)
{
    SW_UNIMPLEMENTED;
    /*auto &os = s.getHostOs();
    if (os.is(OSType::Windows) || os.is(OSType::Cygwin) || os.is(OSType::Mingw))
    {
        // we should pass target settings here and check according target os (cygwin)
        if (os.is(OSType::Cygwin) || os.is(OSType::Mingw) || os.isMingwShell())
            detectNonWindowsCompilers(DETECT_ARGS_PASS);
        detectWindowsCompilers(DETECT_ARGS_PASS);
    }
    else
        detectNonWindowsCompilers(DETECT_ARGS_PASS);
    detectIntelCompilers(DETECT_ARGS_PASS);*/
}

void ProgramDetector::detectProgramsAndLibraries(DETECT_ARGS)
{
#define DETECT(x) detect##x##Compilers(DETECT_ARGS_PASS);
#include "detect.inl"
#undef DETECT
}

// actually we cannot move this to client,
// because we support different languages and packages
// scripting languages do not have os, arch, kernel, configuration etc.
static void addSettings(TargetSettings &ts, bool force)
{
    auto check_and_assign = [force](auto &k, const auto &v)
    {
        if (force || !k)
            k = v;
    };

    // settings
    check_and_assign(ts["native"]["configuration"], "release");
    check_and_assign(ts["native"]["library"], "shared");
    check_and_assign(ts["native"]["mt"], "false");
}

static void setRuleCompareRules(TargetSettings &ts)
{
    // mandatory rules
    for (auto &v : {"c","cpp","link"})
    {
        if (ts["rule"][v])
        {
            ts["rule"].ignoreInComparison(true);
            ts["rule"].useInHash(false);
        }
    }
}

// remember! only host tools
// TODO: load host settings from file
void addSettingsAndSetHostPrograms(const SwCoreContext &swctx, TargetSettings &ts)
{
    addSettings(ts, true);
    return;

    auto to_upkg = [](const auto &s)
    {
        return UnresolvedPackage(s).toString();
    };

    // deps: programs, stdlib etc.
    auto check_and_assign_dependency = [&swctx, &ts](auto &k, const auto &v, int version_level = 0)
    {
        auto check_and_assign = [](auto &k, const auto &v)
        {
            k = v;
        };

        /*auto i = swctx.getPredefinedTargets().find(UnresolvedPackage(v), ts);
        if (i)
            check_and_assign(k, version_level ? i->getPackage().toString(version_level) : i->getPackage().toString());
        else*/
            check_and_assign(k, v);
    };

    if (swctx.getHostOs().Type == OSType::Windows)
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
        //auto cl = swctx.getPredefinedTargets().find(clpkg);

        auto clangpppkg = "org.LLVM.clangpp";
        //auto clangpp = swctx.getPredefinedTargets().find(clpkg);

        if (0);
#ifdef _MSC_VER
        // msvc + clangcl
        // clangcl must be compatible with msvc
        // and also clang actually
        else if (0/*cl != swctx.getPredefinedTargets().end(clpkg) && !cl->second.empty()*/)
        {
            check_and_assign_dependency(ts["native"]["program"]["c"], to_upkg("com.Microsoft.VisualStudio.VC.cl"));
            check_and_assign_dependency(ts["native"]["program"]["cpp"], to_upkg("com.Microsoft.VisualStudio.VC.cl"));
            check_and_assign_dependency(ts["native"]["program"]["asm"], to_upkg("com.Microsoft.VisualStudio.VC.ml"));
            check_and_assign_dependency(ts["native"]["program"]["lib"], to_upkg("com.Microsoft.VisualStudio.VC.lib"));
            check_and_assign_dependency(ts["native"]["program"]["link"], to_upkg("com.Microsoft.VisualStudio.VC.link"));
        }
        // separate?
#else __clang__
        else if (0/*clangpp != swctx.getPredefinedTargets().end(clangpppkg) && !clangpp->second.empty()*/)
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
        //else
            //throw SW_RUNTIME_ERROR("Seems like you do not have Visual Studio installed.\nPlease, install the latest Visual Studio first.");
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
            /*auto &pd = swctx.getPredefinedTargets();
            auto i = pd.find(name);
            if (i == pd.end() || i->second.empty())
                return false;*/
            check_and_assign_dependency(s, name.toString());
            return true;
        };

        auto err_msg = [](const String &cl)
        {
            return "sw was built with " + cl + " as compiler, but it was not found in your system. Install " + cl + " to proceed.";
        };

        // must be the same compiler as current!
#if defined(__clang__)
        bool ok = false;
        if (!(
            if_add(ts["native"]["program"]["c"], "com.Apple.clang"s) &&
            if_add(ts["native"]["program"]["cpp"], "com.Apple.clangpp"s)
            ))
        {
            // no error, we want to check second condition
            //throw SW_RUNTIME_ERROR(err_msg("appleclang"));
        }
        else
            ok = true;
        if (!ok && !(
            if_add(ts["native"]["program"]["c"], "org.LLVM.clang"s) &&
            if_add(ts["native"]["program"]["cpp"], "org.LLVM.clangpp"s)
            ))
        {
            throw SW_RUNTIME_ERROR(err_msg("clang"));
        }
        //if (getHostOs().is(OSType::Linux))
        //ts["native"]["stdlib"]["cpp"] = to_upkg("org.sw.demo.llvm_project.libcxx");
#elif defined(__GNUC__) || defined(__CYGWIN__)
        if (!(
            if_add(ts["native"]["program"]["c"], "org.gnu.gcc"s) &&
            if_add(ts["native"]["program"]["cpp"], "org.gnu.gpp"s)
            ))
        {
            throw SW_RUNTIME_ERROR(err_msg("gcc"));
        }
#elif defined(_WIN32)
#else
#error "Add your current compiler to detect.cpp and here."
#endif

        // using c prog
        if (ts["native"]["program"]["c"].isValue())
            if_add(ts["native"]["program"]["asm"], ts["native"]["program"]["c"].getValue());

        // reconsider, also with driver?
        check_and_assign_dependency(ts["native"]["program"]["lib"], "org.gnu.binutils.ar"s);

        // use driver
        // use cpp driver for the moment to not burden ourselves in adding stdlib
        if (ts["native"]["program"]["cpp"].isValue())
            if_add(ts["native"]["program"]["link"], ts["native"]["program"]["cpp"].getValue());
    }
}

//
void addSettingsAndSetPrograms(const SwCoreContext &swctx, TargetSettings &ts)
{
    addSettings(ts, false);

    auto to_upkg = [](const auto &s)
    {
        return UnresolvedPackage(s).toString();
    };

    // deps: programs, stdlib etc.
    auto check_and_assign_dependency = [&swctx, &ts](auto &k, const auto &v, int version_level = 0)
    {
        auto check_and_assign = [](auto &k, const auto &v, bool force2 = false)
        {
            if (!k || force2)
                k = v;
        };

        bool use_k = k && k.isValue();
        /*auto i = swctx.getPredefinedTargets().find(UnresolvedPackage(use_k ? k.getValue() : v), ts);
        if (i)
            check_and_assign(k, version_level ? i->getPackage().toString(version_level) : i->getPackage().toString(), use_k);
        else*/
            check_and_assign(k, v);
    };

    BuildSettings bs(ts);
    // on win we select msvc, clang, clangcl
    if (bs.TargetOS.is(OSType::Windows))
    {
        //if (!ts["native"]["program"]["c"] || ts["native"]["program"]["c"].isValue())
        String sver;
        if (bs.TargetOS.Version)
            sver = "-" + bs.TargetOS.Version->toString();
        //check_and_assign_dependency(ts["native"]["stdlib"]["c"], to_upkg("com.Microsoft.Windows.SDK.ucrt" + sver));
        //check_and_assign_dependency(ts["native"]["stdlib"]["cpp"], to_upkg("com.Microsoft.VisualStudio.VC.libcpp"));
        //check_and_assign_dependency(ts["native"]["stdlib"]["kernel"], to_upkg("com.Microsoft.Windows.SDK.um" + sver));

        // now find the latest available sdk (ucrt) and select it
        //TargetSettings oss;
        //oss["os"] = ts["os"];
        //auto sdk = swctx.getPredefinedTargets().find(UnresolvedPackage(ts["native"]["stdlib"]["c"].getValue()), oss);
        //if (!sdk)
        //throw SW_RUNTIME_ERROR("No suitable installed WinSDK found for this host");
        //ts["native"]["stdlib"]["c"] = sdk->getPackage().toString(); // assign always
        //ts["os"]["version"] = sdkver->toString(3); // cut off the last (fourth) number

        auto clpkg = "com.Microsoft.VisualStudio.VC.cl";
        //auto cl = swctx.getPredefinedTargets().find(clpkg);

        auto clangpppkg = "org.LLVM.clangpp";
        //auto clangpp = swctx.getPredefinedTargets().find(clpkg);

        auto clangclpkg = "org.LLVM.clangcl";
        //auto clangcl = swctx.getPredefinedTargets().find(clangclpkg);

        if (0);
        // msvc
        else if (getProgramDetector().hasVsInstances())
        {
            ts["rule"]["c"]["package"] = "com.Microsoft.VisualStudio.VC.cl";
            ts["rule"]["c"]["type"] = "msvc";

            ts["rule"]["cpp"]["package"] = "com.Microsoft.VisualStudio.VC.cl";
            ts["rule"]["cpp"]["type"] = "msvc";

            ts["rule"]["asm"]["package"] = "com.Microsoft.VisualStudio.VC.ml";
            ts["rule"]["asm"]["type"] = "msvc";
        }
        // clang
        else if (0/*clangpp != swctx.getPredefinedTargets().end(clangpppkg) && !clangpp->second.empty()*/)
        {
            SW_UNIMPLEMENTED;
            check_and_assign_dependency(ts["native"]["program"]["c"], to_upkg("org.LLVM.clang"));
            check_and_assign_dependency(ts["native"]["program"]["cpp"], to_upkg("org.LLVM.clangpp"));
            check_and_assign_dependency(ts["native"]["program"]["asm"], to_upkg("org.LLVM.clang"));
            // ?
            check_and_assign_dependency(ts["native"]["program"]["lib"], to_upkg("com.Microsoft.VisualStudio.VC.lib"));
            check_and_assign_dependency(ts["native"]["program"]["link"], to_upkg("com.Microsoft.VisualStudio.VC.link"));
        }
        // clangcl
        else if (0/*clangcl != swctx.getPredefinedTargets().end(clangclpkg) && !clangcl->second.empty()*/)
        {
            SW_UNIMPLEMENTED;
            check_and_assign_dependency(ts["native"]["program"]["c"], to_upkg("org.LLVM.clangcl"));
            check_and_assign_dependency(ts["native"]["program"]["cpp"], to_upkg("org.LLVM.clangcl"));
            check_and_assign_dependency(ts["native"]["program"]["asm"], to_upkg("org.LLVM.clangcl"));
            // ?
            check_and_assign_dependency(ts["native"]["program"]["lib"], to_upkg("com.Microsoft.VisualStudio.VC.lib"));
            check_and_assign_dependency(ts["native"]["program"]["link"], to_upkg("com.Microsoft.VisualStudio.VC.link"));
        }
        //else
            //throw SW_RUNTIME_ERROR("No suitable compilers found.\nPlease, install one first.");

        // use msvc's lib and link until llvm tools are not working
        ts["rule"]["lib"]["package"] = "com.Microsoft.VisualStudio.VC.lib";
        ts["rule"]["lib"]["type"] = "msvc";

        ts["rule"]["link"]["package"] = "com.Microsoft.VisualStudio.VC.link";
        ts["rule"]["link"]["type"] = "msvc";

        // always use this rc
        ts["rule"]["rc"]["package"] = "com.Microsoft.Windows.rc";
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
            /*auto &pd = swctx.getPredefinedTargets();
            auto i = pd.find(name);
            if (i == pd.end() || i->second.empty())
                return false;*/
            check_and_assign_dependency(s, name.toString());
            return true;
        };

        auto try_clang = [&if_add, &ts]()
        {
            if_add(ts["native"]["program"]["c"], "org.LLVM.clang"s);
            if_add(ts["native"]["program"]["cpp"], "org.LLVM.clangpp"s);
            if_add(ts["native"]["program"]["c"], "com.Apple.clang"s);
            if_add(ts["native"]["program"]["cpp"], "com.Apple.clangpp"s);
            //if (getHostOs().is(OSType::Linux))
            //ts["native"]["stdlib"]["cpp"] = to_upkg("org.sw.demo.llvm_project.libcxx");
        };

        auto try_gcc = [&if_add, &ts]()
        {
            if_add(ts["native"]["program"]["c"], "org.gnu.gcc"s);
            if_add(ts["native"]["program"]["cpp"], "org.gnu.gpp"s);
        };

        if (bs.TargetOS.is(OSType::Mingw))
            try_gcc();

        try_clang();
        try_gcc();

        // using c prog
        if (ts["native"]["program"]["c"].isValue())
            if_add(ts["native"]["program"]["asm"], ts["native"]["program"]["c"].getValue());

        // reconsider, also with driver?
        check_and_assign_dependency(ts["native"]["program"]["lib"], "org.gnu.binutils.ar"s);

        // use driver
        // use cpp driver for the moment to not burden ourselves in adding stdlib
        if (ts["native"]["program"]["cpp"].isValue())
            if_add(ts["native"]["program"]["link"], ts["native"]["program"]["cpp"].getValue());
    }

    setRuleCompareRules(ts);
}

// they must be the same as used when building sw
void addSettingsAndSetConfigPrograms(const SwContext &swctx, TargetSettings &ts)
{
    ts["native"]["library"] = "static"; // why not shared?
                                        //ts["native"]["mt"] = "true";
    if (swctx.getSettings()["debug_configs"] == "true")
    {
#ifndef NDEBUG
        ts["native"]["configuration"] = "debug";
#else
        ts["native"]["configuration"] = "releasewithdebuginformation";
#endif
    }

#ifdef _MSC_VER
    Version clver(_MSC_VER / 100, _MSC_VER % 100);
#endif
}

}
