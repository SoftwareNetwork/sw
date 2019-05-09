// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "compiler.h"

#include "build.h"
#include "compiler_helpers.h"
#include "target/native.h"

#include <primitives/sw/settings.h>

#ifdef _WIN32
#include "misc/cmVSSetupHelper.h"
#endif

#include <boost/algorithm/string.hpp>

#include <regex>
#include <string>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "compiler");

#define SW_CREATE_COMPILER_COMMAND(t, ct)                                            \
    std::shared_ptr<driver::Command> t::createCommand1(const SwContext &swctx) const \
    {                                                                                \
        auto c = std::make_shared<ct>(swctx);                                        \
        c->fs = fs;                                                                  \
        c->setProgram(file);                                                         \
        return c;                                                                    \
    }

//static cl::opt<bool> do_not_resolve_compiler("do-not-resolve-compiler");
//static cl::opt<bool> use_other_langs("use-other-languages");

// add manual provided options: rust compiler, go compiler, d compiler etc.
// c/cc toolchain probably complex: ar(opt?)+ld(opt)+c+cc(opt)

namespace sw
{

std::string getVsToolset(const Version &v);

void detectNativeCompilers(Build &s);
void detectCSharpCompilers(Build &s);
void detectRustCompilers(Build &s);
void detectGoCompilers(Build &s);
void detectFortranCompilers(Build &s);
void detectJavaCompilers(Build &s);
void detectKotlinCompilers(Build &s);
void detectDCompilers(Build &s);

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

static void add_args(driver::Command &c, const Strings &args)
{
    for (auto &a : args)
        c.args.push_back(a);
}

path getProgramFilesX86();

bool findDefaultVS(path &root, int &VSVersion)
{
    auto program_files_x86 = getProgramFilesX86();
    for (auto &edition : { "Enterprise", "Professional", "Community" })
    {
        for (const auto &[y, v] : std::vector<std::pair<String, int>>{ {"2017", 15}, {"2019", 16} })
        {
            path p = program_files_x86 / ("Microsoft Visual Studio/"s + y + "/"s + edition + "/VC/Auxiliary/Build/vcvarsall.bat");
            if (fs::exists(p))
            {
                root = p.parent_path().parent_path().parent_path();
                VSVersion = v;
                return true;
            }
        }
    }
    return false;
}

void detectCompilers(Build &s)
{
    detectNativeCompilers(s);

    // others
    /*detectCSharpCompilers(s);
    detectRustCompilers(s);
    detectGoCompilers(s);
    detectFortranCompilers(s);
    detectJavaCompilers(s);
    detectKotlinCompilers(s);
    detectDCompilers(s);*/
}

template <class T = PredefinedTarget>
static decltype(auto) addProgram(Build &s, const PackagePath &pp, const std::shared_ptr<Program> &cl)
{
    auto &t = s.add<T>(pp, cl->getVersion());
    t.program = cl;
    t.sw_provided = true;
    return t;
}

void detectDCompilers(Build &s)
{
    path compiler;
    compiler = resolveExecutable("dmd");
    if (compiler.empty())
        return;

    auto C = std::make_shared<DCompiler>(s.swctx);
    C->file = compiler;
    C->Extension = s.getSettings().TargetOS.getExecutableExtension();
    //C->input_extensions = { ".d" };
    addProgram(s, "org.dlang.dmd.dmd", C);
}

void detectKotlinCompilers(Build &s)
{
    path compiler;
    compiler = resolveExecutable("kotlinc");
    if (compiler.empty())
        return;

    auto C = std::make_shared<KotlinCompiler>(s.swctx);
    C->file = compiler;
    //C->input_extensions = { ".kt", ".kts" };
    //s.registerProgram("com.JetBrains.kotlin.kotlinc", C);
}

void detectJavaCompilers(Build &s)
{
    path compiler;
    compiler = resolveExecutable("javac");
    if (compiler.empty())
        return;
    //compiler = resolveExecutable("jar"); // later

    auto C = std::make_shared<JavaCompiler>(s.swctx);
    C->file = compiler;
    //C->input_extensions = { ".java", };
    //s.registerProgram("com.oracle.java.javac", C);
}

void detectFortranCompilers(Build &s)
{
    path compiler;
    compiler = resolveExecutable("gfortran");
    if (compiler.empty())
    {
        compiler = resolveExecutable("f95");
        if (compiler.empty())
        {
            compiler = resolveExecutable("g95");
            if (compiler.empty())
            {
                return;
            }
        }
    }

    auto C = std::make_shared<FortranCompiler>(s.swctx);
    C->file = compiler;
    SW_UNIMPLEMENTED;
    //C->Extension = s.Settings.TargetOS.getExecutableExtension();
    /*C->input_extensions = {
        ".f",
        ".FOR",
        ".for",
        ".f77",
        ".f90",
        ".f95",

        // support Preprocessing
        ".F",
        ".fpp",
        ".FPP",
    };*/
    //s.registerProgram("org.gnu.gcc.fortran", C);
}

void detectGoCompilers(Build &s)
{
#if defined(_WIN32)
    auto compiler = path("go");
    compiler = resolveExecutable(compiler);
    if (compiler.empty())
        return;

    auto C = std::make_shared<GoCompiler>(s.swctx);
    C->file = compiler;
    SW_UNIMPLEMENTED;
    //C->Extension = s.Settings.TargetOS.getExecutableExtension();
    //C->input_extensions = { ".go", };
    //s.registerProgram("org.google.golang.go", C);
#else
#endif
}

void detectRustCompilers(Build &s)
{
#if defined(_WIN32)
    auto compiler = get_home_directory() / ".cargo" / "bin" / "rustc";
    compiler = resolveExecutable(compiler);
    if (compiler.empty())
        return;

    auto C = std::make_shared<RustCompiler>(s.swctx);
    C->file = compiler;
    SW_UNIMPLEMENTED;
    //C->Extension = s.Settings.TargetOS.getExecutableExtension();
    //C->input_extensions = { ".rs", };
    //s.registerProgram("org.rust.rustc", C);
#else
#endif
}

using VSInstances = VersionMap<VSInstance>;

VSInstances &gatherVSInstances(Build &s)
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

            VSInstance inst(s.swctx);
            inst.root = root;
            inst.version = v;
            instances.emplace(v, inst);
        }
#endif
        return instances;
    }();
    return instances;
}

void detectCSharpCompilers(Build &s)
{
    auto &instances = gatherVSInstances(s);
    for (auto &[v, i] : instances)
    {
        auto root = i.root;
        switch (v.getMajor())
        {
        case 15:
            root = root / "MSBuild" / "15.0" / "Bin" / "Roslyn";
            break;
        case 16:
            root = root / "MSBuild" / "Current" / "Bin" / "Roslyn";
            break;
        default:
            SW_UNIMPLEMENTED;
        }

        auto compiler = root / "csc.exe";

        auto C = std::make_shared<VisualStudioCSharpCompiler>(s.swctx);
        C->file = compiler;
        SW_UNIMPLEMENTED;
        //C->Extension = s.Settings.TargetOS.getExecutableExtension();
        //C->input_extensions = { ".cs", };
        //s.registerProgram("com.Microsoft.VisualStudio.Roslyn.csc", C);
    }
}

void detectWindowsCompilers(Build &s)
{
    // we need ifdef because of cmVSSetupAPIHelper
    // but what if we're on Wine?
    // reconsider later

    auto &instances = gatherVSInstances(s);
    for (auto &[_, instance] : instances)
    {
        auto root = instance.root / "VC";
        auto &v = instance.version;

        if (v.getMajor() >= 15)
            root = root / "Tools" / "MSVC" / boost::trim_copy(read_file(root / "Auxiliary" / "Build" / "Microsoft.VCToolsVersion.default.txt"));

        // get suffix
        auto host = toStringWindows(s.getHostOs().Arch);
        auto target = toStringWindows(s.getSettings().TargetOS.Arch);

        auto compiler = root / "bin";
        auto host_root = compiler / ("Host" + host) / host;

        if (v.getMajor() >= 15)
        {
            // always use host tools and host arch for building config files
            compiler /= path("Host" + host) / target / "cl.exe";
        }
        else
        {
            // but we won't detect host&arch stuff on older versions
            compiler /= "cl.exe";
        }

        // create programs

        // lib, link
        {
            auto Linker = std::make_shared<VisualStudioLinker>(s.swctx);
            Linker->Type = LinkerType::MSVC;
            Linker->file = compiler.parent_path() / "link.exe";
            Linker->Extension = s.getSettings().TargetOS.getExecutableExtension();

            if (instance.version.isPreRelease())
                Linker->getVersion().getExtra() = instance.version.getExtra();
            addProgram(s, "com.Microsoft.VisualStudio.VC.link", Linker);
            instance.link_versions.insert(Linker->getVersion());

            if (s.getHostOs().Arch != s.getSettings().TargetOS.Arch)
            {
                auto c = Linker->createCommand(s.swctx);
                c->addPathDirectory(host_root);
            }

            //
            auto Librarian = std::make_shared<VisualStudioLibrarian>(s.swctx);
            Librarian->Type = LinkerType::MSVC;
            Librarian->file = compiler.parent_path() / "lib.exe";
            Librarian->Extension = s.getSettings().TargetOS.getStaticLibraryExtension();

            if (instance.version.isPreRelease())
                Librarian->getVersion().getExtra() = instance.version.getExtra();
            addProgram(s, "com.Microsoft.VisualStudio.VC.lib", Librarian);
            instance.link_versions.insert(Librarian->getVersion());

            if (s.getHostOs().Arch != s.getSettings().TargetOS.Arch)
            {
                auto c = Librarian->createCommand(s.swctx);
                c->addPathDirectory(host_root);
            }

            switch (s.getSettings().TargetOS.Arch)
            {
            case ArchType::x86_64:
                Librarian->Machine = vs::MachineType::X64;
                Linker->Machine = vs::MachineType::X64;
                break;
            case ArchType::x86:
                Librarian->Machine = vs::MachineType::X86;
                Linker->Machine = vs::MachineType::X86;
                break;
            case ArchType::arm:
                Librarian->Machine = vs::MachineType::ARM;
                Linker->Machine = vs::MachineType::ARM;
                break;
            case ArchType::aarch64:
                Librarian->Machine = vs::MachineType::ARM64;
                Linker->Machine = vs::MachineType::ARM64;
                break;
            }
        }

        // ASM
        {
            auto C = std::make_shared<VisualStudioASMCompiler>(s.swctx);
            C->Type = CompilerType::MSVC;
            C->file = s.getSettings().TargetOS.Arch == ArchType::x86_64 ?
                (compiler.parent_path() / "ml64.exe") :
                (compiler.parent_path() / "ml.exe");

            if (instance.version.isPreRelease())
                C->getVersion().getExtra() = instance.version.getExtra();
            //C->input_extensions = { ".asm", };
            addProgram(s, "com.Microsoft.VisualStudio.VC.ml", C);
        }

        // C, C++
        {
            auto exts = getCppSourceFileExtensions();
            exts.insert(".c");

            auto C = std::make_shared<VisualStudioCompiler>(s.swctx);
            C->Type = CompilerType::MSVC;
            C->file = compiler;

            if (instance.version.isPreRelease())
                C->getVersion().getExtra() = instance.version.getExtra();
            //C->input_extensions = exts;
            addProgram(s, "com.Microsoft.VisualStudio.VC.cl", C);
            instance.cl_versions.insert(C->getVersion());

            if (s.getHostOs().Arch != s.getSettings().TargetOS.Arch)
            {
                auto c = C->createCommand(s.swctx);
                c->addPathDirectory(host_root);
            }
        }

        // now register
        addProgram(s, "com.Microsoft.VisualStudio", std::make_shared<VSInstance>(instance));

        continue;

        // clang family

        // create programs
        const path base_llvm_path = path("c:") / "Program Files" / "LLVM";
        const path bin_llvm_path = base_llvm_path / "bin";

        // clang-cl

        // C, C++
        /*{
            auto exts = getCppSourceFileExtensions();
            exts.insert(".c");

            auto C = std::make_shared<ClangClCompiler>(s.swctx);
            C->Type = CompilerType::ClangCl;
            C->file = bin_llvm_path / "clang-cl.exe";
            //C->file = base_llvm_path / "msbuild-bin" / "cl.exe";
            auto COpts2 = COpts;
            // clangcl is able to find VC STL itself
            // also we could provide command line arg -fms-compat...=19.16 19.20 or smth like that
            //COpts2.System.IncludeDirectories.erase(root / "include");
            //COpts2.System.IncludeDirectories.erase(root / "ATLMFC\\include");
            COpts2.System.IncludeDirectories.insert(bin_llvm_path / "lib" / "clang" / C->getVersion().toString() / "include");
            COpts2.System.CompileOptions.push_back("-Wno-everything");
            *C = COpts2;
            //C->input_extensions = exts;
            addProgram(s, "org.LLVM.clangcl", C);

            switch (s.getSettings().TargetOS.Arch)
            {
            case ArchType::x86_64:
                C->CommandLineOptions<ClangClOptions>::Arch = clang::ArchType::m64;
                break;
            case ArchType::x86:
                C->CommandLineOptions<ClangClOptions>::Arch = clang::ArchType::m32;
                break;
            default:
                break;
                //throw SW_RUNTIME_ERROR("Unknown arch");
            }
        }

        // clang

        auto Linker = std::make_shared<VisualStudioLinker>();
        Linker->Type = LinkerType::LLD;
        Linker->file = bin_llvm_path / "lld-link.exe";
        Linker->vs_version = VSVersion;
        *Linker = LOpts;

        auto Librarian = std::make_shared<VisualStudioLibrarian>();
        Librarian->Type = LinkerType::LLD;
        Librarian->file = bin_llvm_path / "llvm-ar.exe"; // ?
        Librarian->vs_version = VSVersion;
        *Librarian = LOpts;

        // C
        {
            auto C = std::make_shared<ClangCompiler>(s.swctx);
            C->Type = CompilerType::Clang;
            C->file = bin_llvm_path / "clang.exe";
            // not available for msvc triple
            // must be enabled on per target basis (when shared lib is built)?
            C->PositionIndependentCode = false;
            auto COpts2 = COpts;
            // is it able to find VC STL itself?
            //COpts2.System.IncludeDirectories.erase(root / "include");
            //COpts2.System.IncludeDirectories.erase(root / "ATLMFC\\include");
            COpts2.System.IncludeDirectories.insert(base_llvm_path / "lib" / "clang" / C->getVersion().toString() / "include");
            COpts2.System.CompileOptions.push_back("-Wno-everything");
            *C = COpts2;
            //C->input_extensions = { ".c", };
            addProgram(s, "org.LLVM.clang", C);
        }

        // C++
        {
            auto C = std::make_shared<ClangCompiler>(s.swctx);
            C->Type = CompilerType::Clang;
            C->file = bin_llvm_path / "clang++.exe";
            // not available for msvc triple
            // must be enabled on per target basis (when shared lib is built)?
            C->PositionIndependentCode = false;
            auto COpts2 = COpts;
            // is it able to find VC STL itself?
            //COpts2.System.IncludeDirectories.erase(root / "include");
            //COpts2.System.IncludeDirectories.erase(root / "ATLMFC\\include");
            COpts2.System.IncludeDirectories.insert(base_llvm_path / "lib" / "clang" / C->getVersion().toString() / "include");
            COpts2.System.CompileOptions.push_back("-Wno-everything");
            *C = COpts2;
            //C->input_extensions = getCppSourceFileExtensions();
            addProgram(s, "org.LLVM.clangpp", C);
        }*/
    }

    // .rc
    {
        auto C = std::make_shared<RcTool>(s.swctx);
        C->file = s.getSettings().Native.SDK.getPath("bin") / toStringWindows(s.getHostOs().Arch) / "rc.exe";
        //for (auto &idir : COpts.System.IncludeDirectories)
            //C->system_idirs.push_back(idir);

        //C->input_extensions = { ".rc", };
        addProgram(s, "com.Microsoft.Windows.rc", C);
    }

    // https://docs.microsoft.com/en-us/cpp/c-runtime-library/crt-library-features?view=vs-2019
    for (auto &[_, instance] : instances)
    {
        auto root = instance.root / "VC";
        auto &v = instance.version;

        if (v.getMajor() >= 15)
            root = root / "Tools" / "MSVC" / boost::trim_copy(read_file(root / "Auxiliary" / "Build" / "Microsoft.VCToolsVersion.default.txt"));

        //auto &vcruntime = s.addLibrary("com.Microsoft.VisualStudio.VC.vcruntime", v);

        auto &libcpp = s.addLibrary("com.Microsoft.VisualStudio.VC.libcpp", v);
        libcpp.AutoDetectOptions = false;
        libcpp.sw_provided = true;
        libcpp.Public.NativeCompilerOptions::System.IncludeDirectories.push_back(root / "include");

        auto &atlmfc = s.addLibrary("com.Microsoft.VisualStudio.VC.ATLMFC", v);
        atlmfc.AutoDetectOptions = false;
        atlmfc.sw_provided = true;
        if (fs::exists(root / "ATLMFC" / "include"))
            atlmfc.Public.NativeCompilerOptions::System.IncludeDirectories.push_back(root / "ATLMFC" / "include");

        // get suffix
        auto host = toStringWindows(s.getHostOs().Arch);
        auto target = toStringWindows(s.getSettings().TargetOS.Arch);

        if (v.getMajor() >= 15)
        {
            libcpp.Public += LinkDirectory(root / "lib" / target);
            if (fs::exists(root / "ATLMFC" / "lib" / target))
                atlmfc.Public += LinkDirectory(root / "ATLMFC" / "lib" / target);
        }
        else
        {
            SW_UNIMPLEMENTED;
        }
    }

    // rename to libc? to crt?
    auto &ucrt = s.addLibrary("com.Microsoft.Windows.SDK.ucrt", s.getSettings().Native.SDK.getWindowsTargetPlatformVersion());
    ucrt.AutoDetectOptions = false;
    ucrt.sw_provided = true;

    // add kits include dirs
    for (auto &i : fs::directory_iterator(s.getSettings().Native.SDK.getPath("Include")))
    {
        if (fs::is_directory(i))
            ucrt.Public.NativeCompilerOptions::System.IncludeDirectories.insert(i);
    }
    for (auto &i : fs::directory_iterator(s.getSettings().Native.SDK.getPath("Lib")))
    {
        if (fs::is_directory(i))
            ucrt.Public.NativeLinkerOptions::System.LinkDirectories.insert(i / toStringWindows(s.getSettings().TargetOS.Arch));
    }

    return;

    throw SW_RUNTIME_ERROR("not implemented");

    // move to gatherVSInstances
    auto find_comn_tools = [](path root, const Version &v) -> std::optional<path>
    {
        auto n = std::to_string(v.getMajor());
        auto ver = "VS"s + n + "COMNTOOLS";
        auto e = getenv(ver.c_str());
        if (e)
        {
            root = e;
            root = root.parent_path().parent_path() / "VC";
            return root;
        }
        return {};
    };

    //!find_comn_tools(VisualStudioVersion::VS16);
    //!find_comn_tools(VisualStudioVersion::VS15);

    path root;
    int v;
    if (findDefaultVS(root, v))
    {
        // find older versions
        for (auto n : {16,15,14,12,11,10,9,8})
        {
            if (find_comn_tools(root, Version(n)))
                break;
        }
    }
}

void detectNonWindowsCompilers(Build &s)
{
    path p;

    NativeLinkerOptions LOpts;

    //LOpts.System.LinkLibraries.push_back("pthread"); // remove and add to progs explicitly?
    //LOpts.System.LinkLibraries.push_back("dl"); // remove and add to progs explicitly?
    //LOpts.System.LinkLibraries.push_back("m"); // remove and add to progs explicitly?

    auto resolve = [](const path &p)
    {
        //if (do_not_resolve_compiler)
            //return p;
        return resolveExecutable(p);
    };

    p = resolve("ar");
    if (!p.empty())
    {
        auto Librarian = std::make_shared<GNULibrarian>(s.swctx);
        Librarian->Type = LinkerType::GNU;
        Librarian->file = p;
        SW_UNIMPLEMENTED;
        //Librarian->Prefix = s.Settings.TargetOS.getLibraryPrefix();
        //Librarian->Extension = s.Settings.TargetOS.getStaticLibraryExtension();
        *Librarian = LOpts;
        //s.registerProgram("org.gnu.binutils.ar", Librarian);
        if (s.getHostOs().is(OSType::Macos))
            Librarian->createCommand(s.swctx)->use_response_files = false;
    }

    FilesOrdered gcc_vers{ "gcc" };
    FilesOrdered gccpp_vers{ "g++" };
    for (int i = 4; i < 12; i++)
    {
        gcc_vers.push_back(path(gcc_vers[0]) += "-" + std::to_string(i));
        gccpp_vers.push_back(path(gccpp_vers[0]) += "-" + std::to_string(i));
    }
    FilesOrdered clang_vers{ "clang" };
    FilesOrdered clangpp_vers{ "clang++" };
    for (int i = 3; i < 16; i++)
    {
        clang_vers.push_back(path(clang_vers[0]) += "-" + std::to_string(i));
        clangpp_vers.push_back(path(clangpp_vers[0]) += "-" + std::to_string(i));
    }
    if (s.getHostOs().is(OSType::Macos))
    {
        // also detect brew
        if (fs::exists("/usr/local/Cellar/llvm"))
        for (auto &d : fs::directory_iterator("/usr/local/Cellar/llvm"))
        {
            clang_vers.push_back(d.path() / "bin/clang");
            clangpp_vers.push_back(d.path() / "bin/clang++");
        }
    }

    //p = resolve("ld.gold");
    for (auto &v : gcc_vers)
    //for (auto &v : gccpp_vers) // this links correct c++ library
    {
        p = resolve(v);
        if (!p.empty())
        {
            auto Linker = std::make_shared<GNULinker>(s.swctx);

            if (s.getHostOs().is(OSType::Macos))
                Linker->use_start_end_groups = false;
            Linker->Type = LinkerType::GNU;
            Linker->file = p;
            SW_UNIMPLEMENTED;
            //Linker->Prefix = s.Settings.TargetOS.getLibraryPrefix();
            //Linker->Extension = s.Settings.TargetOS.getStaticLibraryExtension();

            auto lopts2 = LOpts;
            //lopts2.System.LinkLibraries.push_back("stdc++"); // remove and add to progs explicitly?
            //lopts2.System.LinkLibraries.push_back("stdc++fs"); // remove and add to progs explicitly?

            *Linker = lopts2;
            //s.registerProgram("org.gnu.gcc.ld", Linker);
        }
    }

    NativeCompilerOptions COpts;

    path macos_sdk_dir;
    SW_UNIMPLEMENTED;
    //if (s.Settings.TargetOS.is(OSType::Macos) || s.Settings.TargetOS.is(OSType::IOS))
        //macos_sdk_dir = s.Settings.Native.SDK.getPath();

    auto is_apple_clang = [](const path &p)
    {
        primitives::Command c;
        c.program = p;
        c.args.push_back("--version");
        error_code ec;
        c.execute(ec);
        if (ec)
        {
            LOG_TRACE(logger, "is_apple_clang: not resolved: " + p.u8string());
            return false;
        }
        return c.out.text.find("Apple") != String::npos;
    };

    // ASM
    /*{
        p = resolve("as");

        auto L = std::make_shared<NativeLanguage>();
        //L->Type = LanguageType::ASM;
        // .s - pure asm
        // .S - with #define (accepts -D) and #include (accepts -I), also .sx
        L->CompiledExtensions = { ".s", ".S", ".sx" };
        //s.registerLanguage(L);

        //auto L = (ASMLanguage*)s.languages[LanguageType::ASM].get();
        auto C = std::make_shared<GNUASMCompiler>();
        C->Type = CompilerType::GNU;
        C->file = p;
        *C = COpts;
        L->compiler = C;
        s.registerProgram("org.gnu.gcc.as", C);
    }*/

    for (auto &v : gcc_vers)
    {
        p = resolve(v);
        if (!p.empty())
        {
            // C
            {
                auto C = std::make_shared<GNUCompiler>(s.swctx);
                C->Type = CompilerType::GNU;
                C->file = p;
                *C = COpts;
                // also with asm
                // .s - pure asm
                // .S - with #define (accepts -D) and #include (accepts -I), also .sx
                //C->input_extensions = { ".c", ".s", ".S" };
                //s.registerProgram("org.gnu.gcc.gcc", C);

                if (!macos_sdk_dir.empty())
                    C->IncludeSystemRoot = macos_sdk_dir;
            }
        }
    }

    for (auto &v : gccpp_vers)
    {
        p = resolve(v);
        if (!p.empty())
        {
            // CPP
            {
                auto C = std::make_shared<GNUCompiler>(s.swctx);
                C->Type = CompilerType::GNU;
                C->file = p;
                *C = COpts;
                //C->input_extensions = getCppSourceFileExtensions();
                //s.registerProgram("org.gnu.gcc.gpp", C);

                if (!macos_sdk_dir.empty())
                    C->IncludeSystemRoot = macos_sdk_dir;
            }
        }
    }

    // llvm/clang
    {
        p = resolve("llvm-ar");
        if (!p.empty())
        {
            auto Librarian = std::make_shared<GNULibrarian>(s.swctx);
            Librarian->Type = LinkerType::GNU;
            Librarian->file = p;
            SW_UNIMPLEMENTED;
            //Librarian->Prefix = s.Settings.TargetOS.getLibraryPrefix();
            //Librarian->Extension = s.Settings.TargetOS.getStaticLibraryExtension();
            *Librarian = LOpts;
            //s.registerProgram("org.LLVM.ar", Librarian);
            //if (s.getHostOs().is(OSType::Macos))
            //Librarian->createCommand()->use_response_files = false;
        }

        //p = resolve("ld.gold");
        for (auto &v : clang_vers)
        //for (auto &v : clangpp_vers) // this links correct c++ library
        {
            p = resolve(v);
            if (!p.empty())
            {
                bool appleclang = is_apple_clang(p);

                auto Linker = std::make_shared<GNULinker>(s.swctx);

                if (s.getHostOs().is(OSType::Macos))
                    Linker->use_start_end_groups = false;
                Linker->Type = LinkerType::GNU;
                Linker->file = p;
                SW_UNIMPLEMENTED;
                //Linker->Prefix = s.Settings.TargetOS.getLibraryPrefix();
                //Linker->Extension = s.Settings.TargetOS.getStaticLibraryExtension();

                auto lopts2 = LOpts;
                //lopts2.System.LinkLibraries.push_back("c++"); // remove and add to progs explicitly?
                //lopts2.System.LinkLibraries.push_back("c++fs"); // remove and add to progs explicitly?

                *Linker = lopts2;
                //s.registerProgram(appleclang ? "com.apple.LLVM.ld" : "org.LLVM.ld", Linker);

                if (s.getHostOs().is(OSType::Macos))
                {
                    if (!appleclang)
                        Linker->GNULinkerOptions::LinkDirectories().push_back(p.parent_path().parent_path() / "lib");
                }

                NativeCompilerOptions COpts;

                // C
                {
                    bool appleclang = is_apple_clang(p);

                    auto C = std::make_shared<ClangCompiler>(s.swctx);
                    C->Type = appleclang ? CompilerType::AppleClang : CompilerType::Clang;
                    C->file = p;
                    *C = COpts;
                    // also with asm
                    // .s - pure asm
                    // .S - with #define (accepts -D) and #include (accepts -I), also .sx
                    //C->input_extensions = { ".c", ".s", ".S" };
                    //s.registerProgram(appleclang ? "com.apple.LLVM.clang" : "org.LLVM.clang", C);

                    if (!macos_sdk_dir.empty())
                        C->IncludeSystemRoot = macos_sdk_dir;
                }
            }
        }

        for (auto &v : clangpp_vers)
        {
            p = resolve(v);
            if (!p.empty())
            {
                // CPP
                {
                    bool appleclang = is_apple_clang(p);

                    auto C = std::make_shared<ClangCompiler>(s.swctx);
                    C->Type = appleclang ? CompilerType::AppleClang : CompilerType::Clang;
                    C->file = p;
                    *C = COpts;
                    //C->input_extensions = getCppSourceFileExtensions();
                    //s.registerProgram(appleclang ? "com.apple.LLVM.clangpp" : "org.LLVM.clangpp", C);

                    if (!macos_sdk_dir.empty())
                        C->IncludeSystemRoot = macos_sdk_dir;
                }
            }
        }
    }
}

void detectNativeCompilers(Build &s)
{
    auto &os = s.getSettings().TargetOS;
    if (os.is(OSType::Windows) || os.is(OSType::Cygwin))
    {
        if (os.is(OSType::Cygwin))
            detectNonWindowsCompilers(s);
        detectWindowsCompilers(s);
    }
    else
        detectNonWindowsCompilers(s);
}

void VSInstance::activate(Build &s) const
{
    SW_UNIMPLEMENTED;

    /*if (cl_versions.empty())
        throw SW_RUNTIME_ERROR("missing cl.exe versions");
    if (link_versions.empty())
        throw SW_RUNTIME_ERROR("missing vs tools versions");

    if (!s.activateProgram({ "com.Microsoft.VisualStudio.VC.cl", *cl_versions.rbegin() }, false))
        throw SW_RUNTIME_ERROR("cannot activate com.Microsoft.VisualStudio.VC.cl");
    if (!s.activateProgram({ "com.Microsoft.VisualStudio.VC.ml", *link_versions.rbegin() }, false))
        throw SW_RUNTIME_ERROR("cannot activate com.Microsoft.VisualStudio.VC.ml");

    SW_UNIMPLEMENTED;
    //s.Settings.Native.CompilerType = CompilerType::MSVC;

    // linkers
    auto lib = s.getProgram({ "com.Microsoft.VisualStudio.VC.lib", *link_versions.rbegin() }, false);
    auto link = s.getProgram({ "com.Microsoft.VisualStudio.VC.link", *link_versions.rbegin() }, false);
    auto r = lib && link;
    if (r)
    {
        SW_UNIMPLEMENTED;
        //s.Settings.Native.Librarian = std::dynamic_pointer_cast<NativeLinker>(lib->clone());
        //s.Settings.Native.Linker = std::dynamic_pointer_cast<NativeLinker>(link->clone());
        LOG_TRACE(logger, "activated com.Microsoft.VisualStudio.VC.lib and com.Microsoft.VisualStudio.VC.link successfully");
    }
    else
    {
        if (lib)
            throw SW_RUNTIME_ERROR("cannot activate com.Microsoft.VisualStudio.VC.link");
        else if (link)
            throw SW_RUNTIME_ERROR("cannot activate com.Microsoft.VisualStudio.VC.lib");
        else
            throw SW_RUNTIME_ERROR("cannot activate com.Microsoft.VisualStudio.VC.lib and com.Microsoft.VisualStudio.VC.link");
    }*/
}

path NativeToolchain::SDK::getPath(const path &subdir) const
{
    if (Root.empty())
        throw SW_RUNTIME_ERROR("empty sdk root");
    //if (Version.empty())
        //throw SW_RUNTIME_ERROR("empty sdk version, root is: " + normalize_path(Root));
    if (subdir.empty())
        return Root / Version;
    return Root / Version / subdir / BuildNumber;
}

String getWin10KitDirName();

String NativeToolchain::SDK::getWindowsTargetPlatformVersion() const
{
    if (Version != getWin10KitDirName())
        return Version.u8string();
    return BuildNumber.u8string();
}

void NativeToolchain::SDK::setAndroidApiVersion(int v)
{
    Version = std::to_string(v);
}

bool NativeToolchain::SDK::operator<(const SDK &rhs) const
{
    return std::tie(Root, Version, BuildNumber) < std::tie(rhs.Root, rhs.Version, rhs.BuildNumber);
}

bool NativeToolchain::SDK::operator==(const SDK &rhs) const
{
    return std::tie(Root, Version, BuildNumber) == std::tie(rhs.Root, rhs.Version, rhs.BuildNumber);
}

bool NativeToolchain::operator<(const NativeToolchain &rhs) const
{
    return std::tie(/*CompilerType, */LibrariesType, ConfigurationType, MT, SDK) <
        std::tie(/*rhs.CompilerType, */rhs.LibrariesType, rhs.ConfigurationType, rhs.MT, rhs.SDK);
}

bool NativeToolchain::operator==(const NativeToolchain &rhs) const
{
    return std::tie(/*CompilerType, */LibrariesType, ConfigurationType, MT, SDK) ==
        std::tie(/*rhs.CompilerType, */rhs.LibrariesType, rhs.ConfigurationType, rhs.MT, rhs.SDK);
}

CompilerBaseProgram::CompilerBaseProgram(const CompilerBaseProgram &rhs)
    : FileToFileTransformProgram(rhs)
{
    Prefix = rhs.Prefix;
    Extension = rhs.Extension;
    if (rhs.cmd)
        cmd = rhs.cmd->clone();
}

std::shared_ptr<builder::Command> CompilerBaseProgram::getCommand() const
{
    if (!cmd)
        throw SW_RUNTIME_ERROR("Command is not created");
    if (!prepared)
        throw SW_RUNTIME_ERROR("Command is not prepared");
    return cmd;
}

std::shared_ptr<builder::Command> CompilerBaseProgram::createCommand(const SwContext &swctx)
{
    if (cmd)
        return cmd;
    return cmd = createCommand1(swctx);
}

std::shared_ptr<builder::Command> CompilerBaseProgram::getCommand(const TargetBase &t)
{
    prepareCommand(t);
    return getCommand();
}

std::shared_ptr<builder::Command> CompilerBaseProgram::prepareCommand(const TargetBase &t)
{
    if (prepared)
        return cmd;
    createCommand(t.getSolution().swctx); // do some init
    cmd->fs = t.getSolution().fs;
    prepareCommand1(t);
    prepared = true;
    return cmd;
}

SW_CREATE_COMPILER_COMMAND(CompilerBaseProgram, driver::Command)

std::shared_ptr<SourceFile> CompilerBaseProgram::createSourceFile(const Target &t, const path &input) const
{
    return std::make_shared<SourceFile>(t, input);
}

static Strings getCStdOption(CLanguageStandard std, bool gnuext)
{
    String s = "-std="s + (gnuext ? "gnu" : "c");
    switch (std)
    {
    case CLanguageStandard::C89:
        s += "89";
        break;
    case CLanguageStandard::C99:
        s += "99";
        break;
    case CLanguageStandard::C11:
        s += "11";
        break;
    case CLanguageStandard::C18:
        s += "18";
        break;
    default:
        return {};
    }
    return { s };
}

static Strings getCppStdOption(CPPLanguageStandard std, bool gnuext, bool clang, const Version &clver)
{
    String s = "-std="s + (gnuext ? "gnu" : "c") + "++";
    switch (std)
    {
    case CPPLanguageStandard::CPP11:
        s += "11";
        break;
    case CPPLanguageStandard::CPP14:
        s += "14";
        break;
    case CPPLanguageStandard::CPP17:
        if (clang)
            s += clver > Version(5) ? "17" : "1z";
        else
            s += clver > Version(6) ? "17" : "1z";
        break;
    case CPPLanguageStandard::CPPLatest:
        s += "2a";
        break;
    default:
        return {};
    }
    return { s };
}

String NativeCompiler::getObjectExtension(const OS &o) const
{
    return o.getObjectFileExtension();
}

template <class C>
static path getOutputFile(const Target &t, const C &c, const path &input)
{
    auto o = t.BinaryDir.parent_path() / "obj" /
        (SourceFile::getObjectFilename(t, input) + c.getObjectExtension(t.getSettings().TargetOS));
    o = fs::absolute(o);
    return o;
}

std::shared_ptr<SourceFile> NativeCompiler::createSourceFile(const Target &t, const path &input) const
{
    return std::make_shared<NativeSourceFile>(t, *this, input, ::sw::getOutputFile(t, *this, input));
}

SW_CREATE_COMPILER_COMMAND(VisualStudioCompiler, driver::VSCommand)

void VisualStudioCompiler::prepareCommand1(const TargetBase &t)
{
    if (InputFile)
    {
        cmd->name = normalize_path(InputFile());
        cmd->name_short = InputFile().filename().u8string();
        //cmd->file = InputFile;
    }

    if (CSourceFile)
    {
        cmd->name = normalize_path(CSourceFile());
        cmd->name_short = CSourceFile().filename().u8string();
        //cmd->file = CSourceFile;
    }
    else if (CPPSourceFile)
    {
        cmd->name = normalize_path(CPPSourceFile());
        cmd->name_short = CPPSourceFile().filename().u8string();
        //cmd->file = CPPSourceFile;
    }
    else if (InputFile && !CompileAsC && !CompileAsCPP)
    {
        // .C extension is treated as C language by default (Wt library)
        auto &exts = getCppSourceFileExtensions();
        if (exts.find(InputFile().extension().string()) != exts.end())
        {
            CompileAsCPP = true;
        }
    }

    if (Output)
        cmd->working_directory = Output().parent_path();

    //if (cmd->file.empty())
        //return nullptr;

    //cmd->out.capture = true;

    getCommandLineOptions<VisualStudioCompilerOptions>(cmd.get(), *this);
    addEverything(*cmd);

    if (PreprocessToFile)
    {
        //cmd->addOutput(cmd->file.file.parent_path() / (cmd->file.file.filename().stem().u8string() + ".i"));
        // TODO: remove old object file, it's now incorrect
    }
}

void VisualStudioCompiler::setOutputFile(const path &output_file)
{
    Output = output_file;
}

SW_DEFINE_PROGRAM_CLONE(VisualStudioCompiler)

void VisualStudioCompiler::setSourceFile(const path &input_file, path &output_file)
{
    InputFile = input_file.u8string();
    VisualStudioCompiler::setOutputFile(output_file);
}

path VisualStudioCompiler::getOutputFile() const
{
    return Output();
}

SW_CREATE_COMPILER_COMMAND(VisualStudioASMCompiler, driver::VSCommand)

void VisualStudioASMCompiler::prepareCommand1(const TargetBase &t)
{
    if (file.filename() == "ml64.exe")
        ((VisualStudioASMCompiler*)this)->SafeSEH = false;

    if (InputFile)
    {
        cmd->name = normalize_path(InputFile());
        cmd->name_short = InputFile().filename().u8string();
        //cmd->file = InputFile;
    }
    if (Output)
        cmd->working_directory = Output().parent_path();

    //if (cmd->file.empty())
        //return nullptr;

    //cmd->out.capture = true;
    //cmd->base = clone();

    // defs and idirs for asm must go before file
    addEverything(*cmd);
    getCommandLineOptions<VisualStudioAssemblerOptions>(cmd.get(), *this);
}

SW_DEFINE_PROGRAM_CLONE(VisualStudioASMCompiler)

void VisualStudioASMCompiler::setOutputFile(const path &output_file)
{
    Output = output_file;
}

path VisualStudioASMCompiler::getOutputFile() const
{
    return Output();
}

void VisualStudioASMCompiler::setSourceFile(const path &input_file, path &output_file)
{
    InputFile = input_file.u8string();
    setOutputFile(output_file);
}

SW_CREATE_COMPILER_COMMAND(ClangCompiler, driver::GNUCommand)

void ClangCompiler::prepareCommand1(const TargetBase &t)
{
    auto cmd = std::static_pointer_cast<driver::GNUCommand>(this->cmd);

    if (InputFile)
    {
        cmd->name = normalize_path(InputFile());
        cmd->name_short = InputFile().filename().u8string();
        //cmd->file = InputFile;
    }
    if (OutputFile)
    {
        cmd->deps_file = OutputFile().parent_path() / (OutputFile().stem().u8string() + ".d");
        cmd->working_directory = OutputFile().parent_path();
    }

    add_args(*cmd, getCStdOption(CStandard(), dynamic_cast<const NativeCompiledTarget&>(t).CExtensions));
    CStandard.skip = true;
    add_args(*cmd, getCppStdOption(CPPStandard(), dynamic_cast<const NativeCompiledTarget&>(t).CPPExtensions,
        true, getVersion()));
    CPPStandard.skip = true;

    getCommandLineOptions<ClangOptions>(cmd.get(), *this);
    addEverything(*this->cmd);
    getCommandLineOptions<ClangOptions>(cmd.get(), *this, "", true);
}

void ClangCompiler::setOutputFile(const path &output_file)
{
    OutputFile = output_file;
}

path ClangCompiler::getOutputFile() const
{
    return OutputFile();
}

SW_DEFINE_PROGRAM_CLONE(ClangCompiler)

void ClangCompiler::setSourceFile(const path &input_file, path &output_file)
{
    InputFile = input_file.u8string();
    setOutputFile(output_file);
}

SW_CREATE_COMPILER_COMMAND(ClangClCompiler, driver::VSCommand)

void ClangClCompiler::prepareCommand1(const TargetBase &t)
{
    if (InputFile)
    {
        cmd->name = normalize_path(InputFile());
        cmd->name_short = InputFile().filename().u8string();
        //cmd->file = InputFile;
    }
    if (CSourceFile)
    {
        cmd->name = normalize_path(CSourceFile());
        cmd->name_short = CSourceFile().filename().u8string();
        //cmd->file = CSourceFile;
    }
    if (CPPSourceFile)
    {
        cmd->name = normalize_path(CPPSourceFile());
        cmd->name_short = CPPSourceFile().filename().u8string();
        //cmd->file = CPPSourceFile;
    }
    if (Output)
        cmd->working_directory = Output().parent_path();

    //if (cmd->file.empty())
        //return nullptr;

    //cmd->out.capture = true;
    //cmd->base = clone();

    add_args(*cmd, getCStdOption(dynamic_cast<const NativeCompiledTarget&>(t).CVersion,
        dynamic_cast<const NativeCompiledTarget&>(t).CExtensions));
    add_args(*cmd, getCppStdOption(CPPStandard(), dynamic_cast<const NativeCompiledTarget&>(t).CPPExtensions,
        true, getVersion()));
    CPPStandard.skip = true;

    getCommandLineOptions<VisualStudioCompilerOptions>(cmd.get(), *this);
    getCommandLineOptions<ClangClOptions>(cmd.get(), *this/*, "-Xclang"*/);
    addEverything(*cmd);
}

void ClangClCompiler::setOutputFile(const path &output_file)
{
    Output = output_file;
}

path ClangClCompiler::getOutputFile() const
{
    return Output();
}

SW_DEFINE_PROGRAM_CLONE(ClangClCompiler)

void ClangClCompiler::setSourceFile(const path &input_file, path &output_file)
{
    InputFile = input_file.u8string();
    setOutputFile(output_file);
}

SW_CREATE_COMPILER_COMMAND(GNUASMCompiler, driver::GNUCommand)

void GNUASMCompiler::prepareCommand1(const TargetBase &t)
{
    bool assembly = false;
    if (InputFile)
    {
        cmd->name = normalize_path(InputFile());
        cmd->name_short = InputFile().filename().u8string();
        //cmd->file = InputFile;
        assembly = InputFile().extension() == ".s";
    }
    if (OutputFile)
        cmd->working_directory = OutputFile().parent_path();

    //if (cmd->file.empty())
        //return nullptr;

    //cmd->out.capture = true;

    getCommandLineOptions<GNUAssemblerOptions>(cmd.get(), *this);

    if (!InputFile && !assembly)
        addEverything(*cmd);
}

SW_DEFINE_PROGRAM_CLONE(GNUASMCompiler)

void GNUASMCompiler::setOutputFile(const path &output_file)
{
    OutputFile = output_file;
}

path GNUASMCompiler::getOutputFile() const
{
    return OutputFile();
}

void GNUASMCompiler::setSourceFile(const path &input_file, path &output_file)
{
    InputFile = input_file.u8string();
    setOutputFile(output_file);
}

SW_DEFINE_PROGRAM_CLONE(ClangASMCompiler)

SW_CREATE_COMPILER_COMMAND(GNUCompiler, driver::GNUCommand)

void GNUCompiler::prepareCommand1(const TargetBase &t)
{
    auto cmd = std::static_pointer_cast<driver::GNUCommand>(this->cmd);

    if (InputFile)
    {
        cmd->name = normalize_path(InputFile());
        cmd->name_short = InputFile().filename().u8string();
        //cmd->file = InputFile;
    }
    if (OutputFile)
    {
        cmd->deps_file = OutputFile().parent_path() / (OutputFile().stem().u8string() + ".d");
        cmd->working_directory = OutputFile().parent_path();
    }

    //if (cmd->file.empty())
        //return nullptr;

    //cmd->out.capture = true;

    add_args(*cmd, getCStdOption(CStandard(), dynamic_cast<const NativeCompiledTarget&>(t).CExtensions));
    CStandard.skip = true;
    add_args(*cmd, getCppStdOption(CPPStandard(), dynamic_cast<const NativeCompiledTarget&>(t).CPPExtensions,
        false, getVersion()));
    CPPStandard.skip = true;

    getCommandLineOptions<GNUOptions>(cmd.get(), *this);
    addEverything(*this->cmd);
    getCommandLineOptions<GNUOptions>(cmd.get(), *this, "", true);
}

void GNUCompiler::setOutputFile(const path &output_file)
{
    OutputFile = output_file;
}

path GNUCompiler::getOutputFile() const
{
    return OutputFile();
}

SW_DEFINE_PROGRAM_CLONE(GNUCompiler)

void GNUCompiler::setSourceFile(const path &input_file, path &output_file)
{
    InputFile = input_file.u8string();
    setOutputFile(output_file);
}

FilesOrdered NativeLinker::gatherLinkDirectories() const
{
    FilesOrdered dirs;

    auto get_ldir = [&dirs](const auto &a)
    {
        for (auto &d : a)
            dirs.push_back(d);
    };

    get_ldir(NativeLinkerOptions::gatherLinkDirectories());
    get_ldir(NativeLinkerOptions::System.gatherLinkDirectories());

    return dirs;
}

FilesOrdered NativeLinker::gatherLinkLibraries(bool system) const
{
    FilesOrdered dirs;

    auto get_ldir = [&dirs](const auto &a)
    {
        for (auto &d : a)
            dirs.push_back(d);
    };

    if (system)
        get_ldir(NativeLinkerOptions::System.gatherLinkLibraries());
    else
        get_ldir(NativeLinkerOptions::gatherLinkLibraries());

    return dirs;
}

void VisualStudioLibraryTool::setObjectFiles(const Files &files)
{
    InputFiles().insert(files.begin(), files.end());
}

void VisualStudioLibraryTool::setOutputFile(const path &out)
{
    Output = out.u8string() + Extension;
}

void VisualStudioLibraryTool::setImportLibrary(const path &out)
{
    ImportLibrary = out.u8string() + ".lib";
}

path VisualStudioLibraryTool::getOutputFile() const
{
    return Output;
}

path VisualStudioLibraryTool::getImportLibrary() const
{
    if (ImportLibrary)
        return ImportLibrary();
    path p = Output;
    return p.parent_path() / (p.filename().stem() += ".lib");
}

void VisualStudioLibraryTool::prepareCommand1(const TargetBase &t)
{
    // can be zero imput files actually: lib.exe /DEF:my.def /OUT:x.lib
    //if (InputFiles().empty())
        //return nullptr;

    //LinkDirectories() = gatherLinkDirectories();
    //LinkLibraries() = gatherLinkLibraries();

    //cmd->out.capture = true;
    //cmd->base = clone();
    if (Output)
    {
        cmd->working_directory = Output().parent_path();
        cmd->name = normalize_path(Output());
        cmd->name_short = Output().filename().u8string();
    }

    ((VisualStudioLibraryTool*)this)->VisualStudioLibraryToolOptions::LinkDirectories() = gatherLinkDirectories();

    getCommandLineOptions<VisualStudioLibraryToolOptions>(cmd.get(), *this);
    addEverything(*cmd);
    getAdditionalOptions(cmd.get());
}

SW_DEFINE_PROGRAM_CLONE(VisualStudioLinker)

void VisualStudioLinker::getAdditionalOptions(driver::Command *cmd) const
{
    getCommandLineOptions<VisualStudioLinkerOptions>(cmd, *this);
}

void VisualStudioLinker::setInputLibraryDependencies(const FilesOrdered &files)
{
    InputLibraryDependencies().insert(InputLibraryDependencies().end(), files.begin(), files.end());
}

void VisualStudioLinker::prepareCommand1(const TargetBase &t)
{
    // can be zero imput files actually: lib.exe /DEF:my.def /OUT:x.lib
    //if (InputFiles().empty())
        //return nullptr;

    //LinkDirectories() = gatherLinkDirectories();
    //LinkLibraries() = gatherLinkLibraries();
    ((VisualStudioLinker*)this)->VisualStudioLinkerOptions::SystemLinkLibraries = gatherLinkLibraries(true);

    //cmd->out.capture = true;
    //cmd->base = clone();
    if (Output)
    {
        cmd->working_directory = Output().parent_path();
        cmd->name = normalize_path(Output());
        cmd->name_short = Output().filename().u8string();
    }

    ((VisualStudioLibraryTool*)this)->VisualStudioLibraryToolOptions::LinkDirectories() = gatherLinkDirectories();

    getCommandLineOptions<VisualStudioLibraryToolOptions>(cmd.get(), *this);
    addEverything(*cmd);
    getAdditionalOptions(cmd.get());
}

SW_DEFINE_PROGRAM_CLONE(VisualStudioLibrarian)

void VisualStudioLibrarian::getAdditionalOptions(driver::Command *cmd) const
{
    getCommandLineOptions<VisualStudioLibrarianOptions>(cmd, *this);
}

// https://dev.gentoo.org/~vapier/crt.txt
// http://gcc.gnu.org/onlinedocs/gccint/Initialization.html

SW_DEFINE_PROGRAM_CLONE(GNULinker)

void GNULinker::setObjectFiles(const Files &files)
{
    InputFiles().insert(files.begin(), files.end());
}

static auto add_prefix_and_suffix(const path &p, const String &prefix, const String &ext)
{
    return p.parent_path() / (prefix + p.filename().u8string() + ext);
}

static auto remove_prefix_and_suffix(const path &p)
{
    auto s = p.stem().u8string();
    if (s.find("lib") == 0)
        s = s.substr(3);
    return s;
}

void GNULinker::setOutputFile(const path &out)
{
    Output = add_prefix_and_suffix(out, Prefix, Extension).u8string();
}

void GNULinker::setImportLibrary(const path &out)
{
    //ImportLibrary = out.u8string();// + ".lib";
}

void GNULinker::setLinkLibraries(const FilesOrdered &in)
{
    for (auto &lib : in)
        NativeLinker::LinkLibraries.push_back(lib);
}

void GNULinker::setInputLibraryDependencies(const FilesOrdered &files)
{
    if (files.empty())
		return;
    // TODO: fast fix for GNU
    // https://eli.thegreenplace.net/2013/07/09/library-order-in-static-linking
    if (use_start_end_groups)
        StartGroup = true;
    InputLibraryDependencies().insert(InputLibraryDependencies().end(), files.begin(), files.end());
    if (use_start_end_groups)
        EndGroup = true;
}

path GNULinker::getOutputFile() const
{
    return Output;
}

path GNULinker::getImportLibrary() const
{
    //if (ImportLibrary)
        //return ImportLibrary();
    //path p = Output;
    //return p.parent_path() / (p.filename().stem() += ".a");
    return Output;
}

void GNULinker::getAdditionalOptions(driver::Command *cmd) const
{
    getCommandLineOptions<GNULinkerOptions>(cmd, *this);
}

void GNULinker::prepareCommand1(const TargetBase &t)
{
    // can be zero imput files actually: lib.exe /DEF:my.def /OUT:x.lib
    //if (InputFiles().empty())
        //return nullptr;

    //LinkDirectories() = gatherLinkDirectories();
    //((GNULinker*)this)->GNULinkerOptions::LinkLibraries() = gatherLinkLibraries();
    ((GNULinker*)this)->GNULinkerOptions::SystemLinkLibraries = gatherLinkLibraries(true);

    //if (t.getSolution().getHostOs().is(OSType::Windows))
    {
        // lld will add windows absolute paths to libraries
        //
        //  ldd -d test-0.0.1
        //      linux-vdso.so.1 (0x00007ffff724c000)
        //      D:\temp\9\musl\.sw\linux_x86_64_clang_9.0_shared_Release\musl-1.1.21.so => not found
        //      D:\temp\9\musl\.sw\linux_x86_64_clang_9.0_shared_Release\compiler_rt.builtins-0.0.1.so => not found
        //
        // so we strip abs paths and pass them to -L

        UniqueVector<path> dirs;
        auto &origin_dirs = GNULinkerOptions::LinkDirectories();
        for (auto &d : origin_dirs)
            dirs.push_back(d);

        auto update_libs = [&dirs, this](auto &a, bool add_inputs = false)
        {
            for (auto &ll : a)
            {
                if (ll.is_relative())
                    continue;
                if (add_inputs)
                    cmd->addInput(ll);
                dirs.insert(ll.parent_path());
                ll = "-l" + remove_prefix_and_suffix(ll);
            }
        };

        // we also now provide manual handling of input files

        update_libs(NativeLinker::LinkLibraries);
        update_libs(NativeLinker::System.LinkLibraries);
        update_libs(GNULinkerOptions::InputLibraryDependencies(), true);
        update_libs(GNULinkerOptions::LinkLibraries(), true);
        update_libs(GNULinkerOptions::SystemLinkLibraries());

        GNULinkerOptions::InputLibraryDependencies.input_dependency = false;
        GNULinkerOptions::LinkLibraries.input_dependency = false;

        origin_dirs.clear();
        for (auto &d : dirs)
            origin_dirs.push_back(d);

        // remove later?
        cmd->args.push_back("-rpath");
        cmd->args.push_back("./");
    }

    //cmd->out.capture = true;
    //cmd->base = clone();
    if (Output)
    {
        cmd->working_directory = Output().parent_path();
        cmd->name = normalize_path(Output());
        cmd->name_short = Output().filename().u8string();
    }

    //((GNULibraryTool*)this)->GNULibraryToolOptions::LinkDirectories() = gatherLinkDirectories();

    getCommandLineOptions<GNULinkerOptions>(cmd.get(), *this);
    addEverything(*cmd);
    //getAdditionalOptions(cmd.get());
}

SW_DEFINE_PROGRAM_CLONE(GNULibrarian)

void GNULibrarian::setObjectFiles(const Files &files)
{
    InputFiles().insert(files.begin(), files.end());
}

void GNULibrarian::setOutputFile(const path &out)
{
    Output = add_prefix_and_suffix(out, Prefix, Extension).u8string();
}

void GNULibrarian::setImportLibrary(const path &out)
{
    //ImportLibrary = out.u8string();// + ".lib";
}

path GNULibrarian::getOutputFile() const
{
    return Output;
}

path GNULibrarian::getImportLibrary() const
{
    //if (ImportLibrary)
        //return ImportLibrary();
    path p = Output;
    return p.parent_path() / (p.filename().stem() += ".a");
}

void GNULibrarian::getAdditionalOptions(driver::Command *cmd) const
{
    getCommandLineOptions<GNULibrarianOptions>(cmd, *this);
}

void GNULibrarian::prepareCommand1(const TargetBase &t)
{
    // these's some issue with archives not recreated, but keeping old symbols
    // TODO: investigate, fix and remove?
    cmd->remove_outputs_before_execution = true;

    // can be zero imput files actually: lib.exe /DEF:my.def /OUT:x.lib
    //if (InputFiles().empty())
        //return nullptr;

    //LinkDirectories() = gatherLinkDirectories();
    //LinkLibraries() = gatherLinkLibraries();

    //cmd->out.capture = true;
    //cmd->base = clone();
    if (Output)
    {
        cmd->working_directory = Output().parent_path();
        cmd->name = normalize_path(Output());
        cmd->name_short = Output().filename().u8string();
    }

    //((GNULibraryTool*)this)->GNULibraryToolOptions::LinkDirectories() = gatherLinkDirectories();

    getCommandLineOptions<GNULibrarianOptions>(cmd.get(), *this);
    addEverything(*cmd);
    //getAdditionalOptions(cmd.get());
}

SW_DEFINE_PROGRAM_CLONE(RcTool)

void RcTool::prepareCommand1(const TargetBase &t)
{
    cmd->protect_args_with_quotes = false;

    if (InputFile)
    {
        cmd->name = normalize_path(InputFile());
        cmd->name_short = InputFile().filename().u8string();
    }

    t.template asRef<NativeCompiledTarget>().NativeCompilerOptions::addDefinitionsAndIncludeDirectories(*cmd);

    // ms bug: https://developercommunity.visualstudio.com/content/problem/417189/rcexe-incorrect-behavior-with.html
    //for (auto &i : system_idirs)
        //cmd->args.push_back("-I" + normalize_path(i));

    // use env
    String s;
    for (auto &i : system_idirs)
        s += normalize_path(i) + ";";
    cmd->environment["INCLUDE"] = s;

    // fix spaces around defs value:
    // from: -DSW_PACKAGE_API=extern \"C\" __declspec(dllexport)
    // to:   -DSW_PACKAGE_API="extern \"C\" __declspec(dllexport)"

    // find better way - protect things in addEverything?

    for (auto &a : cmd->args)
    {
        if (a.find("-D") == 0)
        {
            auto ep = a.find("=");
            if (ep == a.npos || a.find(" ") == a.npos)
                continue;
            if (a.size() == ep || a[ep + 1] == '\"')
                continue;
            a = a.substr(0, ep) + "=\"" + a.substr(ep + 1) + "\"";
        }
        if (a.find("-I") == 0)
        {
            if (a.find(" ") == a.npos)
                continue;
            a = "-I\"" + a.substr(2) + "\"";
        }
    }

    getCommandLineOptions<RcToolOptions>(cmd.get(), *this);
}

void RcTool::setOutputFile(const path &output_file)
{
    Output = output_file;
}

void RcTool::setSourceFile(const path &input_file)
{
    InputFile = input_file;
}

std::shared_ptr<SourceFile> RcTool::createSourceFile(const Target &t, const path &input) const
{
    return std::make_shared<RcToolSourceFile>(t, *this, input, ::sw::getOutputFile(t, *this, input));
}

SW_DEFINE_PROGRAM_CLONE(VisualStudioCSharpCompiler)

void VisualStudioCSharpCompiler::prepareCommand1(const TargetBase &t)
{
    getCommandLineOptions<VisualStudioCSharpCompilerOptions>(cmd.get(), *this);
}

void VisualStudioCSharpCompiler::setOutputFile(const path &output_file)
{
    Output = output_file.u8string() + Extension;
}

void VisualStudioCSharpCompiler::addSourceFile(const path &input_file)
{
    InputFiles().insert(input_file);
}

SW_DEFINE_PROGRAM_CLONE(RustCompiler)

void RustCompiler::prepareCommand1(const TargetBase &t)
{
    getCommandLineOptions<RustCompilerOptions>(cmd.get(), *this);
}

void RustCompiler::setOutputFile(const path &output_file)
{
    Output = output_file.u8string() + Extension;
}

void RustCompiler::setSourceFile(const path &input_file)
{
    InputFile() = input_file;
}

SW_DEFINE_PROGRAM_CLONE(GoCompiler)

void GoCompiler::prepareCommand1(const TargetBase &t)
{
    getCommandLineOptions<GoCompilerOptions>(cmd.get(), *this);
}

void GoCompiler::setOutputFile(const path &output_file)
{
    Output = output_file.u8string() + Extension;
}

void GoCompiler::setSourceFile(const path &input_file)
{
    InputFiles().insert(input_file);
}

SW_DEFINE_PROGRAM_CLONE(FortranCompiler)

void FortranCompiler::prepareCommand1(const TargetBase &t)
{
    getCommandLineOptions<FortranCompilerOptions>(cmd.get(), *this);
}

void FortranCompiler::setOutputFile(const path &output_file)
{
    Output = output_file.u8string() + Extension;
}

void FortranCompiler::setSourceFile(const path &input_file)
{
    InputFiles().insert(input_file);
}

SW_DEFINE_PROGRAM_CLONE(JavaCompiler)

void JavaCompiler::prepareCommand1(const TargetBase &t)
{
    getCommandLineOptions<JavaCompilerOptions>(cmd.get(), *this);

    for (auto &f : InputFiles())
    {
        auto o = OutputDir() / (f.filename().stem() += ".class");
        File(o, *fs).addImplicitDependency(f);
        cmd->addOutput(o);
    }
}

void JavaCompiler::setOutputDir(const path &output_dir)
{
    OutputDir = output_dir;
}

void JavaCompiler::setSourceFile(const path &input_file)
{
    InputFiles().insert(input_file);
}

SW_DEFINE_PROGRAM_CLONE(KotlinCompiler)

void KotlinCompiler::prepareCommand1(const TargetBase &t)
{
    getCommandLineOptions<KotlinCompilerOptions>(cmd.get(), *this);
}

void KotlinCompiler::setOutputFile(const path &output_file)
{
    Output = output_file;
    Output() += ".jar";
}

void KotlinCompiler::setSourceFile(const path &input_file)
{
    InputFiles().insert(input_file);
}

SW_DEFINE_PROGRAM_CLONE(DCompiler)

void DCompiler::prepareCommand1(const TargetBase &t)
{
    getCommandLineOptions<DCompilerOptions>(cmd.get(), *this);
}

void DCompiler::setOutputFile(const path &output_file)
{
    Output = output_file.u8string() + Extension;
}

void DCompiler::setObjectDir(const path &output_dir)
{
    ObjectDir = output_dir;
}

void DCompiler::setSourceFile(const path &input_file)
{
    InputFiles().insert(input_file);
}

}
