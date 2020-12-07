// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include "detect.h"

#include "compiler.h"
#include "../misc/cmVSSetupHelper.h"
#include "../build.h"
#include "../command.h"
#include "../program_version_storage.h"
#include "../options_cl_vs.h"
#include "../rule.h"

#include <boost/algorithm/string.hpp>
#include <primitives/command.h>

#include <regex>
#include <string>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "detect");

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
    s.merge(getProgramDetector().detectWindowsCompilers());
    return s;
}

void ProgramDetector::log_msg_detect_target(const String &m)
{
    //LOG_TRACE(logger, m);
}

PredefinedProgramTarget &ProgramDetector::addProgram(DETECT_ARGS, const PackageId &id, const PackageSettings &ts, const Program &p)
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
    root = i.root / "VC";
    if (is_vs15plus())
        root = root / "Tools" / "MSVC" / boost::trim_copy(read_file(root / "Auxiliary" / "Build" / "Microsoft.VCToolsVersion.default.txt"));
    compiler = root / "bin";
    idir = root / "include";
}

void ProgramDetector::MsvcInstance::process(DETECT_ARGS)
{
    auto &eb = static_cast<ExtendedBuild &>(b);
    BuildSettings new_settings = eb.getSettings();
    const auto host = toStringWindows(b.getContext().getHostOs().Arch);

    // get suffix
    target_arch = new_settings.TargetOS.Arch;
    if (is_vs15plus())
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
    auto p = std::make_unique<SimpleProgram>();
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

bool ProgramDetector::MsvcInstance::is_vs15plus() const
{
    return i.version.getMajor() >= 15;
}

bool ProgramDetector::MsvcInstance::has_no_target_libdir() const
{
    return !is_vs15plus() && target == "x86";
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
    eps.emplace("com.Microsoft.VisualStudio.VC.cl", [this, inm = m](DETECT_ARGS)
    {
        auto &eb = static_cast<ExtendedBuild &>(b);
        auto m = inm;
        m.process(DETECT_ARGS_PASS);

        auto p = std::make_unique<VisualStudioCompiler>();
        p->file = m.compiler / "cl.exe";
        if (!fs::exists(p->file))
            return;

        if (b.getContext().getHostOs().Arch != m.target_arch)
        {
            auto c = p->getCommand();
            c->addPathDirectory(m.host_root);
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
    // actually we can use all host link program
    eps.emplace("com.Microsoft.VisualStudio.VC.link", [this, inm = m](DETECT_ARGS)
    {
        auto &eb = static_cast<ExtendedBuild &>(b);
        auto m = inm;
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

    // actually we can use all host lib program
    eps.emplace("com.Microsoft.VisualStudio.VC.lib", [this, inm = m](DETECT_ARGS)
    {
        auto &eb = static_cast<ExtendedBuild &>(b);
        auto m = inm;
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
    eps.emplace("com.Microsoft.VisualStudio.VC.ml", [this, inm = m](DETECT_ARGS)
    {
        auto &eb = static_cast<ExtendedBuild &>(b);
        auto m = inm;
        m.process(DETECT_ARGS_PASS);

        if (m.target_arch == ArchType::x86_64 || m.target_arch == ArchType::x86)
        {
            auto p = std::make_unique<VisualStudioASMCompiler>();
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
        auto p = std::make_unique<SimpleProgram>();
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
    eps.emplace("com.Microsoft.VisualStudio.VC.libcpp", [this, inm = m](DETECT_ARGS)
    {
        auto &eb = static_cast<ExtendedBuild &>(b);
        BuildSettings new_settings = eb.getSettings();
        auto m = inm;
        m.process(DETECT_ARGS_PASS);

        auto &libcpp = addTarget<PredefinedTarget>(DETECT_ARGS_PASS, PackageId("com.Microsoft.VisualStudio.VC.libcpp", m.cl_exe_version), eb.getSettings());
        libcpp.public_ts["properties"]["6"]["system_include_directories"].push_back(m.idir);
        if (m.has_no_target_libdir())
            libcpp.public_ts["properties"]["6"]["system_link_directories"].push_back(m.root / "lib");
        else
            libcpp.public_ts["properties"]["6"]["system_link_directories"].push_back(m.root / "lib" / m.target);
        if (m.cl_exe_version.getMajor() >= 19)
        {
            // we also add some other libs needed by msvc
            // oldnames.lib - for backward compat - https://docs.microsoft.com/en-us/cpp/c-runtime-library/backward-compatibility?view=vs-2019

            // under cond?
            libcpp.public_ts["properties"]["6"]["system_link_libraries"].push_back(path(boost::to_upper_copy("oldnames.lib"s)));

            // 100% under cond
            libcpp.public_ts["properties"]["6"]["system_link_libraries"].push_back(path(boost::to_upper_copy("legacy_stdio_definitions.lib"s)));
            libcpp.public_ts["properties"]["6"]["system_link_libraries"].push_back(path(boost::to_upper_copy("legacy_stdio_wide_specifiers.lib"s)));
        }
        switch (sw::getProgramDetector().getMsvcLibraryType(new_settings))
        {
        case vs::RuntimeLibraryType::MultiThreadedDLL:
            libcpp.public_ts["properties"]["6"]["system_link_libraries"].push_back(path(boost::to_upper_copy("msvcprt.lib"s)));
            break;
        case vs::RuntimeLibraryType::MultiThreadedDLLDebug:
            libcpp.public_ts["properties"]["6"]["system_link_libraries"].push_back(path(boost::to_upper_copy("msvcprtd.lib"s)));
            break;
        case vs::RuntimeLibraryType::MultiThreaded:
            libcpp.public_ts["properties"]["6"]["system_link_libraries"].push_back(path(boost::to_upper_copy("libcpmt.lib"s)));
            break;
        case vs::RuntimeLibraryType::MultiThreadedDebug:
            libcpp.public_ts["properties"]["6"]["system_link_libraries"].push_back(path(boost::to_upper_copy("libcpmtd.lib"s)));
            break;
        default:
            SW_UNIMPLEMENTED;
        }
        // add to separate lib?
        switch (sw::getProgramDetector().getMsvcLibraryType(new_settings))
        {
        case vs::RuntimeLibraryType::MultiThreadedDLL:
            libcpp.public_ts["properties"]["6"]["system_link_libraries"].push_back(path(boost::to_upper_copy("msvcrt.lib"s)));
            break;
        case vs::RuntimeLibraryType::MultiThreadedDLLDebug:
            libcpp.public_ts["properties"]["6"]["system_link_libraries"].push_back(path(boost::to_upper_copy("msvcrtd.lib"s)));
            break;
        case vs::RuntimeLibraryType::MultiThreaded:
            libcpp.public_ts["properties"]["6"]["system_link_libraries"].push_back(path(boost::to_upper_copy("libcmt.lib"s)));
            break;
        case vs::RuntimeLibraryType::MultiThreadedDebug:
            libcpp.public_ts["properties"]["6"]["system_link_libraries"].push_back(path(boost::to_upper_copy("libcmtd.lib"s)));
            break;
        default:
            SW_UNIMPLEMENTED;
        }
    });

    eps.emplace("com.Microsoft.VisualStudio.VC.ATLMFC", [this, inm = m](DETECT_ARGS)
    {
        if (!fs::exists(inm.root / "ATLMFC" / "include"))
            return;

        auto &eb = static_cast<ExtendedBuild &>(b);
        auto m = inm;
        m.process(DETECT_ARGS_PASS);
        auto &atlmfc = addTarget<PredefinedTarget>(DETECT_ARGS_PASS, PackageId("com.Microsoft.VisualStudio.VC.ATLMFC", m.cl_exe_version), eb.getSettings());
        atlmfc.public_ts["properties"]["6"]["system_include_directories"].push_back(m.root / "ATLMFC" / "include");
        if (m.has_no_target_libdir())
            atlmfc.public_ts["properties"]["6"]["system_link_directories"].push_back(m.root / "ATLMFC" / "lib");
        else
            atlmfc.public_ts["properties"]["6"]["system_link_directories"].push_back(m.root / "ATLMFC" / "lib" / m.target);
        atlmfc.public_ts["properties"]["6"]["system_link_libraries"].push_back(boost::to_upper_copy("atls.lib"s));
    });

    // concrt
    eps.emplace("com.Microsoft.VisualStudio.VC.concrt", [this, inm = m](DETECT_ARGS)
    {
        if (!fs::exists(inm.root / "crt" / "src" / "concrt"))
            return;

        auto &eb = static_cast<ExtendedBuild &>(b);
        BuildSettings new_settings = eb.getSettings();
        auto m = inm;
        m.process(DETECT_ARGS_PASS);

        if (m.cl_exe_version.getMajor() < 19)
            return;

        auto &concrt = addTarget<PredefinedTarget>(DETECT_ARGS_PASS, PackageId("com.Microsoft.VisualStudio.VC.concrt", m.cl_exe_version), eb.getSettings());
        // protected?
        concrt.public_ts["properties"]["6"]["system_include_directories"].push_back(m.root / "crt" / "src" / "concrt");
        concrt.public_ts["properties"]["6"]["system_link_libraries"].push_back(
            path(boost::to_upper_copy(sw::getProgramDetector().getMsvcLibraryName("concrt", new_settings))));
    });

    // vcruntime
    eps.emplace("com.Microsoft.VisualStudio.VC.runtime", [this, inm = m](DETECT_ARGS)
    {
        if (!fs::exists(inm.root / "crt" / "src" / "vcruntime"))
            return;

        auto &eb = static_cast<ExtendedBuild &>(b);
        BuildSettings new_settings = eb.getSettings();
        auto m = inm;
        m.process(DETECT_ARGS_PASS);

        if (m.cl_exe_version.getMajor() < 19)
            return;

        auto &vcruntime = addTarget<PredefinedTarget>(DETECT_ARGS_PASS, PackageId("com.Microsoft.VisualStudio.VC.runtime", m.cl_exe_version), eb.getSettings());
        // protected?
        //vcruntime.public_ts["properties"]["6"]["system_include_directories"].push_back(root / "crt" / "src" / "vcruntime");
        vcruntime.public_ts["properties"]["6"]["system_link_libraries"].push_back(
            path(boost::to_upper_copy(sw::getProgramDetector().getMsvcLibraryName("vcruntime", new_settings))));
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

ProgramDetector::DetectablePackageMultiEntryPoints ProgramDetector::detectWindowsClang()
{
    DetectablePackageMultiEntryPoints eps;

    static const path base_llvm_path = path("c:") / "Program Files" / "LLVM";
    static const path bin_llvm_path = base_llvm_path / "bin";
    static bool colored_output = hasConsoleColorProcessing();

    // clang-cl

    // C, C++
    eps.emplace("org.LLVM.clangcl", [this](DETECT_ARGS)
    {
        auto p = std::make_unique<ClangClCompiler>();
        p->file = bin_llvm_path / "clang-cl.exe";
        //C->file = base_llvm_path / "msbuild-bin" / "cl.exe";
        if (!fs::exists(p->file))
        {
            static auto f = resolveExecutable("clang-cl");
            if (!fs::exists(f))
                return;
            p->file = f;
        }
        auto cmd = p->getCommand();
        cmd->setProgram(p->file);
        auto msvc_prefix = getMsvcPrefix(*cmd);
        getMsvcIncludePrefixes()[p->file] = msvc_prefix;

        auto [o, v] = getVersionAndOutput(b.getContext(), p->file);

        // check before adding target
        static std::regex r("InstalledDir: (.*)\\r?\\n?");
        std::smatch m;
        if (!std::regex_search(o, m, r))
        {
            LOG_ERROR(logger, "Cannot get clang-cl installed dir (InstalledDir): " + o);
            return;
        }

        auto &c = addProgram(DETECT_ARGS_PASS, PackageId("org.LLVM.clangcl", v), {}, *p);
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

        // rules
        auto ru = std::make_unique<NativeCompilerRule>(p->clone());
        ru->lang = NativeCompilerRule::LANG_C;
        c.setRule("c", std::move(ru));
        ru = std::make_unique<NativeCompilerRule>(p->clone());
        ru->lang = NativeCompilerRule::LANG_CPP;
        c.setRule("cpp", std::move(ru));
    });

    // clang

    // link
    eps.emplace("org.LLVM.lld", [this](DETECT_ARGS)
    {
        auto p = std::make_unique<SimpleProgram>();
        p->file = bin_llvm_path / "lld.exe";
        if (!fs::exists(p->file))
        {
            auto f = resolveExecutable("lld");
            if (!fs::exists(f))
                return;
            p->file = f;
        }
        auto v = getVersion(b.getContext(), p->file);
        addProgram(DETECT_ARGS_PASS, PackageId("org.LLVM.lld", v), {}, *p);
    });

    // lld-link
    eps.emplace("org.LLVM.lld.link", [this](DETECT_ARGS)
    {
        auto p = std::make_unique<SimpleProgram>();
        p->file = bin_llvm_path / "lld-link.exe";
        if (!fs::exists(p->file))
        {
            auto f = resolveExecutable("lld-link");
            if (!fs::exists(f))
                return;
            p->file = f;
        }

        auto v = getVersion(b.getContext(), p->file);
        addProgram(DETECT_ARGS_PASS, PackageId("org.LLVM.lld.link", v), {}, *p);

        auto c2 = p->getCommand();
        c2->push_back("-lldignoreenv"); // prevents libs dirs autodetection (from msvc)
    });

    // ar
    eps.emplace("org.LLVM.ar", [this](DETECT_ARGS)
    {
        auto p = std::make_unique<SimpleProgram>();
        p->file = bin_llvm_path / "llvm-ar.exe";
        if (!fs::exists(p->file))
        {
            auto f = resolveExecutable("llvm-ar");
            if (!fs::exists(f))
                return;
            p->file = f;
        }
        auto v = getVersion(b.getContext(), p->file);
        addProgram(DETECT_ARGS_PASS, PackageId("org.LLVM.ar", v), {}, *p);
    });

    // C
    eps.emplace("org.LLVM.clang", [this](DETECT_ARGS)
    {
        auto p = std::make_unique<ClangCompiler>();
        p->file = bin_llvm_path / "clang.exe";
        if (!fs::exists(p->file))
        {
            auto f = resolveExecutable("clang");
            if (!fs::exists(f))
                return;
            p->file = f;
        }

        auto v = getVersion(b.getContext(), p->file);
        auto &c = addProgram(DETECT_ARGS_PASS, PackageId("org.LLVM.clang", v), {}, *p);

        if (colored_output)
        {
            auto c2 = p->getCommand();
            c2->push_back("-fcolor-diagnostics");
            c2->push_back("-fansi-escape-codes");
        }
        //c->push_back("-Wno-everything");
        // is it able to find VC STL itself?
        //COpts2.System.IncludeDirectories.insert(base_llvm_path / "lib" / "clang" / C->getVersion().toString() / "include");

        auto ru = std::make_unique<NativeCompilerRule>(p->clone());
        ru->lang = NativeCompilerRule::LANG_C;
        c.setRule("c", std::move(ru));
        ru = std::make_unique<NativeCompilerRule>(p->clone());
        ru->lang = NativeCompilerRule::LANG_ASM;
        c.setRule("asm", std::move(ru));
    });

    // C++
    eps.emplace("org.LLVM.clangpp", [this](DETECT_ARGS)
    {
        auto p = std::make_unique<ClangCompiler>();
        p->file = bin_llvm_path / "clang++.exe";
        if (!fs::exists(p->file))
        {
            auto f = resolveExecutable("clang++");
            if (!fs::exists(f))
                return;
            p->file = f;
        }

        auto v = getVersion(b.getContext(), p->file);
        auto &c = addProgram(DETECT_ARGS_PASS, PackageId("org.LLVM.clangpp", v), {}, *p);

        if (colored_output)
        {
            auto c2 = p->getCommand();
            c2->push_back("-fcolor-diagnostics");
            c2->push_back("-fansi-escape-codes");
        }
        //c->push_back("-Wno-everything");
        // is it able to find VC STL itself?
        //COpts2.System.IncludeDirectories.insert(base_llvm_path / "lib" / "clang" / C->getVersion().toString() / "include");

        auto ru = std::make_unique<NativeCompilerRule>(p->clone());
        ru->lang = NativeCompilerRule::LANG_CPP;
        c.setRule("cpp", std::move(ru));
    });

    return eps;
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
            auto p = std::make_unique<SimpleProgram>();
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
            auto p = std::make_unique<SimpleProgram>(); // new object
            p->file = resolveExecutable("icc");
            if (fs::exists(p->file))
            {
                auto v = getVersion(s, p->file);
                addProgram(DETECT_ARGS_PASS, PackageId("com.intel.compiler.c", v), {}, p);
            }
        }

        {
            auto p = std::make_unique<SimpleProgram>(); // new object
            p->file = resolveExecutable("icpc");
            if (fs::exists(p->file))
            {
                auto v = getVersion(s, p->file);
                addProgram(DETECT_ARGS_PASS, PackageId("com.intel.compiler.cpp", v), {}, p);
            }
        }
    }*/
}

ProgramDetector::DetectablePackageMultiEntryPoints ProgramDetector::detectWindowsCompilers()
{
    DetectablePackageMultiEntryPoints eps;
    eps.merge(detectMsvc());
    eps.merge(detectWindowsClang());
    return eps;
}

void ProgramDetector::detectNonWindowsCompilers(DETECT_ARGS)
{
    bool colored_output = hasConsoleColorProcessing();

    SW_UNIMPLEMENTED;
    /*auto resolve_and_add = [DETECT_ARGS_PASS_TO_LAMBDA, &colored_output]
    (const path &prog, const String &ppath, int color_diag = 0, const String &regex_prefix = {})
    {
        auto p = std::make_unique<SimpleProgram>();
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
    nc.appleclang = true;

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

}
