// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "compiler.h"

#include "../build.h"
#include "compiler_helpers.h"
#include "../target/native.h"

#include <sw/core/sw_context.h>

#include <primitives/sw/settings.h>

#ifdef _WIN32
#include "misc/cmVSSetupHelper.h"
#endif

#include <boost/algorithm/string.hpp>

#include <regex>
#include <string>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "compiler.detect");

//static cl::opt<bool> do_not_resolve_compiler("do-not-resolve-compiler");

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

struct SomeSettingsSettingsComparator : SettingsComparator
{
    void addSetting(const String &in) { return s.push_back(in); }

    bool equal(const TargetSettings &s1, const TargetSettings &s2) const override
    {
        return all_of(s.begin(), s.end(), [&s1, &s2](auto &s)
        {
            return s1[s] == s2[s];
        });
    }

private:
    Strings s;
};

struct LinkerSettingsComparator : SomeSettingsSettingsComparator
{
    LinkerSettingsComparator()
    {
        addSetting("os.kernel");
        addSetting("os.arch");
    }
};

using CompilerSettingsComparator = LinkerSettingsComparator;

// left join comparator

template <class T = PredefinedTarget>
static decltype(auto) addProgramNoFile(Build &s, const PackagePath &pp, const std::shared_ptr<Program> &p)
{
    auto &t = s.add<T>(pp, p->getVersion());
    t.setProgram(p);
    t.sw_provided = true;
    return t;
}

template <class T = PredefinedTarget>
static decltype(auto) addProgram(Build &s, const PackagePath &pp, const std::shared_ptr<Program> &p)
{
    //if (!fs::exists(p->file))
        //throw SW_RUNTIME_ERROR("Program does not exist: " + normalize_path(p->file));
    return addProgramNoFile(s, pp, p);
}

void detectDCompilers(Build &s)
{
    path compiler;
    compiler = resolveExecutable("dmd");
    if (compiler.empty())
        return;

    auto C = std::make_shared<DCompiler>(s.swctx);
    C->file = compiler;
    C->Extension = s.getBuildSettings().TargetOS.getExecutableExtension();
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
    const auto host = toStringWindows(s.getHostOs().Arch);

    for (auto target_arch : {ArchType::x86_64,ArchType::x86,ArchType::arm,ArchType::aarch64})
    {
        auto new_settings = s.getBuildSettings();
        new_settings.TargetOS.Arch = target_arch;

        for (auto &[_, instance] : instances)
        {
            auto root = instance.root / "VC";
            auto &v = instance.version;

            if (v.getMajor() >= 15)
                root = root / "Tools" / "MSVC" / boost::trim_copy(read_file(root / "Auxiliary" / "Build" / "Microsoft.VCToolsVersion.default.txt"));

            // get suffix
            auto target = toStringWindows(target_arch);

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
                Linker->Extension = s.getBuildSettings().TargetOS.getExecutableExtension();

                if (instance.version.isPreRelease())
                    Linker->getVersion().getExtra() = instance.version.getExtra();
                auto &link = addProgram(s, "com.Microsoft.VisualStudio.VC.link", Linker);
                link.ts = new_settings.getTargetSettings();
                link.setSettingsComparator(std::make_unique<LinkerSettingsComparator>());
                instance.link_versions.insert(Linker->getVersion());

                if (s.getHostOs().Arch != target_arch)
                {
                    auto c = Linker->createCommand(s.swctx);
                    c->addPathDirectory(host_root);
                }

                //
                auto Librarian = std::make_shared<VisualStudioLibrarian>(s.swctx);
                Librarian->Type = LinkerType::MSVC;
                Librarian->file = compiler.parent_path() / "lib.exe";
                Librarian->Extension = s.getBuildSettings().TargetOS.getStaticLibraryExtension();

                if (instance.version.isPreRelease())
                    Librarian->getVersion().getExtra() = instance.version.getExtra();
                auto &lib = addProgram(s, "com.Microsoft.VisualStudio.VC.lib", Librarian);
                lib.ts = new_settings.getTargetSettings();
                lib.setSettingsComparator(std::make_unique<LinkerSettingsComparator>());
                instance.link_versions.insert(Librarian->getVersion());

                if (s.getHostOs().Arch != target_arch)
                {
                    auto c = Librarian->createCommand(s.swctx);
                    c->addPathDirectory(host_root);
                }
            }

            // ASM
            if (target_arch == ArchType::x86_64 || target_arch == ArchType::x86)
            {
                auto C = std::make_shared<VisualStudioASMCompiler>(s.swctx);
                C->Type = CompilerType::MSVC;
                C->file = target_arch == ArchType::x86_64 ?
                    (compiler.parent_path() / "ml64.exe") :
                    (compiler.parent_path() / "ml.exe");

                if (instance.version.isPreRelease())
                    C->getVersion().getExtra() = instance.version.getExtra();
                auto &ml = addProgram(s, "com.Microsoft.VisualStudio.VC.ml", C);
                ml.ts = new_settings.getTargetSettings();
                ml.setSettingsComparator(std::make_unique<CompilerSettingsComparator>());
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
                auto &cl = addProgram(s, "com.Microsoft.VisualStudio.VC.cl", C);
                cl.ts = new_settings.getTargetSettings();
                cl.setSettingsComparator(std::make_unique<CompilerSettingsComparator>());
                instance.cl_versions.insert(C->getVersion());

                if (s.getHostOs().Arch != target_arch)
                {
                    auto c = C->createCommand(s.swctx);
                    c->addPathDirectory(host_root);
                }
            }

            // now register
            addProgramNoFile(s, "com.Microsoft.VisualStudio", std::make_shared<VSInstance>(instance));

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

                switch (target_arch)
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
    }

    // .rc
    {
        auto C = std::make_shared<RcTool>(s.swctx);
        C->file = s.getBuildSettings().Native.SDK.getPath("bin") / toStringWindows(s.getHostOs().Arch) / "rc.exe";
        //for (auto &idir : COpts.System.IncludeDirectories)
            //C->system_idirs.push_back(idir);

        //C->input_extensions = { ".rc", };
        auto &rc = addProgram(s, "com.Microsoft.Windows.rc", C);
        rc.setSettingsComparator(std::make_unique<CompilerSettingsComparator>());
    }

    // https://docs.microsoft.com/en-us/cpp/c-runtime-library/crt-library-features?view=vs-2019
    for (auto target_arch : { ArchType::x86_64,ArchType::x86,ArchType::arm,ArchType::aarch64 })
    {
        auto new_settings = s.getBuildSettings();
        new_settings.TargetOS.Arch = target_arch;

        for (auto &[_, instance] : instances)
        {
            auto root = instance.root / "VC";
            auto &v = instance.version;

            if (v.getMajor() >= 15)
                root = root / "Tools" / "MSVC" / boost::trim_copy(read_file(root / "Auxiliary" / "Build" / "Microsoft.VCToolsVersion.default.txt"));

            //auto &vcruntime = s.addLibrary("com.Microsoft.VisualStudio.VC.vcruntime", v);

            auto libcppt = s.make<LibraryTarget>("com.Microsoft.VisualStudio.VC.libcpp", v);
            libcppt->HeaderOnly = true;
            libcppt->ts = new_settings.getTargetSettings();
            while (libcppt->init());
            auto &libcpp = s.registerTarget(libcppt);
            libcpp.AutoDetectOptions = false;
            libcpp.sw_provided = true;
            libcpp.setSettingsComparator(std::make_unique<CompilerSettingsComparator>());
            libcpp.Public.NativeCompilerOptions::System.IncludeDirectories.push_back(root / "include");

            auto &atlmfct = s.make<LibraryTarget>("com.Microsoft.VisualStudio.VC.ATLMFC", v);
            atlmfct->HeaderOnly = true;
            atlmfct->ts = new_settings.getTargetSettings();
            while (atlmfct->init());
            auto &atlmfc = s.registerTarget(atlmfct);
            atlmfc.AutoDetectOptions = false;
            atlmfc.sw_provided = true;
            atlmfc.setSettingsComparator(std::make_unique<CompilerSettingsComparator>());
            if (fs::exists(root / "ATLMFC" / "include"))
                atlmfc.Public.NativeCompilerOptions::System.IncludeDirectories.push_back(root / "ATLMFC" / "include");

            // get suffix
            auto target = toStringWindows(target_arch);

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

            // early prepare
            while (libcpp.prepare());
            while (atlmfc.prepare());
        }

        // rename to libc? to crt?
        auto &ucrtt = s.make<LibraryTarget>("com.Microsoft.Windows.SDK.ucrt", s.getBuildSettings().Native.SDK.getWindowsTargetPlatformVersion());
        ucrtt->HeaderOnly = true;
        ucrtt->ts = new_settings.getTargetSettings();
        while (ucrtt->init());
        auto &ucrt = s.registerTarget(ucrtt);
        ucrt.AutoDetectOptions = false;
        ucrt.sw_provided = true;
        auto ucmp = std::make_unique<CompilerSettingsComparator>();
        ucmp->addSetting("os.version");
        ucrt.setSettingsComparator(std::move(ucmp));

        // add kits include dirs
        for (auto &i : fs::directory_iterator(s.getBuildSettings().Native.SDK.getPath("Include")))
        {
            if (fs::is_directory(i))
                ucrt.Public.NativeCompilerOptions::System.IncludeDirectories.insert(i);
        }
        for (auto &i : fs::directory_iterator(s.getBuildSettings().Native.SDK.getPath("Lib")))
        {
            if (fs::is_directory(i))
                ucrt.Public.NativeLinkerOptions::System.LinkDirectories.insert(i / toStringWindows(target_arch));
        }
        // early prepare
        while (ucrt.prepare());
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
        c.setProgram(p);
        c.arguments.push_back("--version");
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
    auto &os = s.getBuildSettings().TargetOS;
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

}
