// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <compiler.h>

#include <language.h>
#include <solution.h>
#include <compiler_helpers.h>
#include <target/native.h>

#include <primitives/sw/settings.h>

#ifdef _WIN32
#include <misc/cmVSSetupHelper.h>
#endif

#include <boost/algorithm/string.hpp>

#include <regex>
#include <string>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "compiler");

#define SW_MAKE_COMPILER_COMMAND_WITH_FILE(t) \
    SW_MAKE_COMPILER_COMMAND(t)

#define SW_CREATE_COMPILER_COMMAND(t, m, ct)                        \
    std::shared_ptr<driver::Command> t::createCommand1() const \
    {                                                               \
        m(ct);                                                      \
        return c;                                                   \
    }

//static cl::opt<bool> do_not_resolve_compiler("do-not-resolve-compiler");
//static cl::opt<bool> use_other_langs("use-other-languages");

// add manual provided options: rust compiler, go compiler, d compiler etc.
// c/cc toolchain probably complex: ar(opt?)+ld(opt)+c+cc(opt)

namespace sw
{

std::string getVsToolset(const Version &v);

void detectNativeCompilers(struct Solution &s);
void detectCSharpCompilers(struct Solution &s);
void detectRustCompilers(struct Solution &s);
void detectGoCompilers(struct Solution &s);
void detectFortranCompilers(struct Solution &s);
void detectJavaCompilers(struct Solution &s);
void detectKotlinCompilers(struct Solution &s);
void detectDCompilers(struct Solution &s);

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

void detectCompilers(struct Solution &s)
{
    detectNativeCompilers(s);

    // others
    detectCSharpCompilers(s);
    detectRustCompilers(s);
    detectGoCompilers(s);
    detectFortranCompilers(s);
    detectJavaCompilers(s);
    detectKotlinCompilers(s);
    detectDCompilers(s);
}

void detectDCompilers(struct Solution &s)
{
    path compiler;
    compiler = resolveExecutable("dmd");
    if (compiler.empty())
        return;

    auto L = std::make_shared<DLanguage>();
    L->CompiledExtensions = { ".d" };

    auto C = std::make_shared<DCompiler>();
    C->file = compiler;
    C->Extension = s.Settings.TargetOS.getExecutableExtension();
    L->compiler = C;
    s.registerProgramAndLanguage("org.dlang.dmd.dmd", C, L);
}

void detectKotlinCompilers(struct Solution &s)
{
    path compiler;
    compiler = resolveExecutable("kotlinc");
    if (compiler.empty())
        return;

    auto L = std::make_shared<KotlinLanguage>();
    L->CompiledExtensions = { ".kt", ".kts" };

    auto C = std::make_shared<KotlinCompiler>();
    C->file = compiler;
    L->compiler = C;
    s.registerProgramAndLanguage("com.JetBrains.kotlin.kotlinc", C, L);
}

void detectJavaCompilers(struct Solution &s)
{
    path compiler;
    compiler = resolveExecutable("javac");
    if (compiler.empty())
        return;
    //compiler = resolveExecutable("jar"); // later

    auto L = std::make_shared<JavaLanguage>();
    L->CompiledExtensions = { ".java", };

    auto C = std::make_shared<JavaCompiler>();
    C->file = compiler;
    L->compiler = C;
    s.registerProgramAndLanguage("com.oracle.java.javac", C, L);
}

void detectFortranCompilers(struct Solution &s)
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

    auto L = std::make_shared<FortranLanguage>();
    L->CompiledExtensions = {
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
    };

    auto C = std::make_shared<FortranCompiler>();
    C->file = compiler;
    C->Extension = s.Settings.TargetOS.getExecutableExtension();
    L->compiler = C;
    s.registerProgramAndLanguage("org.gnu.gcc.fortran", C, L);
}

void detectGoCompilers(struct Solution &s)
{
#if defined(_WIN32)
    auto compiler = path("go");
    compiler = resolveExecutable(compiler);
    if (compiler.empty())
        return;

    auto L = std::make_shared<GoLanguage>();
    L->CompiledExtensions = { ".go" };

    auto C = std::make_shared<GoCompiler>();
    C->file = compiler;
    C->Extension = s.Settings.TargetOS.getExecutableExtension();
    L->compiler = C;
    s.registerProgramAndLanguage("org.google.golang.go", C, L);
#else
#endif
}

void detectRustCompilers(struct Solution &s)
{
#if defined(_WIN32)
    auto compiler = get_home_directory() / ".cargo" / "bin" / "rustc";
    compiler = resolveExecutable(compiler);
    if (compiler.empty())
        return;

    auto L = std::make_shared<RustLanguage>();
    L->CompiledExtensions = { ".rs" };

    auto C = std::make_shared<RustCompiler>();
    C->file = compiler;
    C->Extension = s.Settings.TargetOS.getExecutableExtension();
    L->compiler = C;
    s.registerProgramAndLanguage("org.rust.rustc", C, L);
#else
#endif
}

using VSInstances = VersionMap<VSInstance>;

VSInstances &gatherVSInstances()
{
    static VSInstances instances = []()
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
            instances[v] = inst;
        }
#endif
        return instances;
    }();
    return instances;
}

void detectCSharpCompilers(struct Solution &s)
{
    auto &instances = gatherVSInstances();
    for (auto &[v, i] : instances)
    {
        auto root = i.root;
        /*if (v.getMajor() == 15)
        {*/
            root = root / "MSBuild" / "15.0" / "Bin" / "Roslyn";
        /*}
        if (v.getMajor() == 16)
        {
            root = root / "MSBuild" / "Current" / "Bin" / "Roslyn";
        }*/

        auto compiler = root / "csc.exe";

        auto L = std::make_shared<CSharpLanguage>();
        L->CompiledExtensions = { ".cs" };

        auto C = std::make_shared<VisualStudioCSharpCompiler>();
        C->file = compiler;
        C->Extension = s.Settings.TargetOS.getExecutableExtension();
        L->compiler = C;
        s.registerProgramAndLanguage("com.Microsoft.VisualStudio.Roslyn.csc", C, L);
    }
}

void detectWindowsCompilers(struct Solution &s)
{
    // we need ifdef because of cmVSSetupAPIHelper
    // but what if we're on Wine?
    // reconsider later

    auto &instances = gatherVSInstances();
    for (auto &[_, instance] : instances)
    {
        auto root = instance.root / "VC";
        auto &v = instance.version;

        if (v.getMajor() >= 15)
            root = root / "Tools\\MSVC" / boost::trim_copy(read_file(root / "Auxiliary\\Build\\Microsoft.VCToolsVersion.default.txt"));

        auto compiler = root / "bin";
        NativeCompilerOptions COpts;
        COpts.System.IncludeDirectories.insert(root / "include");
        COpts.System.IncludeDirectories.insert(root / "ATLMFC\\include"); // also add

        struct DirSuffix
        {
            std::string host;
            std::string target;
        } dir_suffix;

        // get suffix
        dir_suffix.host = toStringWindows(s.HostOS.Arch);
        dir_suffix.target = toStringWindows(s.Settings.TargetOS.Arch);

        auto host_root = compiler / ("Host" + dir_suffix.host) / dir_suffix.host;
        NativeLinkerOptions LOpts;

        // continue
        if (v.getMajor() >= 15)
        {
            // always use host tools and host arch for building config files
            compiler /= "Host" + dir_suffix.host + "\\" + dir_suffix.target + "\\cl.exe";
            LOpts.System.LinkDirectories.insert(root / ("lib\\" + dir_suffix.target));
            LOpts.System.LinkDirectories.insert(root / ("ATLMFC\\lib\\" + dir_suffix.target)); // also add
        }
        else
        {
            // but we won't detect host&arch stuff on older versions
            compiler /= "cl.exe";
        }

        // add kits include dirs
        for (auto &i : fs::directory_iterator(s.Settings.Native.SDK.getPath("Include")))
        {
            if (fs::is_directory(i))
                COpts.System.IncludeDirectories.insert(i);
        }
        for (auto &i : fs::directory_iterator(s.Settings.Native.SDK.getPath("Lib")))
        {
            if (fs::is_directory(i))
                LOpts.System.LinkDirectories.insert(i / path(dir_suffix.target));
        }

        // create programs

        // lib, link
        {
            auto Linker = std::make_shared<VisualStudioLinker>();
            Linker->Type = LinkerType::MSVC;
            Linker->file = compiler.parent_path() / "link.exe";
            //Linker->vs_version = VSVersion;
            Linker->Extension = s.Settings.TargetOS.getExecutableExtension();
            *Linker = LOpts;

            if (instance.version.isPreRelease())
                Linker->getVersion().getExtra() = instance.version.getExtra();
            s.registerProgram("com.Microsoft.VisualStudio.VC.link", Linker);
            instance.link_versions.insert(Linker->getVersion());

            if (s.HostOS.Arch != s.Settings.TargetOS.Arch)
            {
                auto c = Linker->createCommand();
                c->addPathDirectory(host_root);
            }

            //
            auto Librarian = std::make_shared<VisualStudioLibrarian>();
            Librarian->Type = LinkerType::MSVC;
            Librarian->file = compiler.parent_path() / "lib.exe";
            //Librarian->vs_version = VSVersion;
            Librarian->Extension = s.Settings.TargetOS.getStaticLibraryExtension();
            *Librarian = LOpts;

            if (instance.version.isPreRelease())
                Librarian->getVersion().getExtra() = instance.version.getExtra();
            s.registerProgram("com.Microsoft.VisualStudio.VC.lib", Librarian);
            instance.link_versions.insert(Librarian->getVersion());

            if (s.HostOS.Arch != s.Settings.TargetOS.Arch)
            {
                auto c = Librarian->createCommand();
                c->addPathDirectory(host_root);
            }

            switch (s.Settings.TargetOS.Arch)
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
            auto L = std::make_shared<NativeLanguage>();
            L->CompiledExtensions = { ".asm" };

            auto C = std::make_shared<VisualStudioASMCompiler>();
            C->Type = CompilerType::MSVC;
            C->file = s.Settings.TargetOS.Arch == ArchType::x86_64 ?
                (compiler.parent_path() / "ml64.exe") :
                (compiler.parent_path() / "ml.exe");
            *C = COpts;
            L->compiler = C;

            if (instance.version.isPreRelease())
                C->getVersion().getExtra() = instance.version.getExtra();
            s.registerProgramAndLanguage("com.Microsoft.VisualStudio.VC.ml", C, L);
        }

        // C, C++
        {
            auto L = std::make_shared<NativeLanguage>();
            L->CompiledExtensions = getCppSourceFileExtensions();
            L->CompiledExtensions.insert(".c");

            auto C = std::make_shared<VisualStudioCompiler>();
            C->Type = CompilerType::MSVC;
            C->file = compiler;
            *C = COpts;
            L->compiler = C;

            if (instance.version.isPreRelease())
                C->getVersion().getExtra() = instance.version.getExtra();
            s.registerProgramAndLanguage("com.Microsoft.VisualStudio.VC.cl", C, L);
            instance.cl_versions.insert(C->getVersion());

            if (s.HostOS.Arch != s.Settings.TargetOS.Arch)
            {
                auto c = C->createCommand();
                c->addPathDirectory(host_root);
            }
        }

        // now register
        s.registerProgram("com.Microsoft.VisualStudio", std::make_shared<VSInstance>(instance));

        // .rc
        {
            auto L = std::make_shared<RcToolLanguage>();
            L->CompiledExtensions = { ".rc" };

            auto C = std::make_shared<RcTool>();
            C->file = s.Settings.Native.SDK.getPath("bin") / dir_suffix.host / "rc.exe";
            for (auto &idir : COpts.System.IncludeDirectories)
                C->system_idirs.push_back(idir);

            L->compiler = C;
            s.registerProgramAndLanguage("com.Microsoft.Windows.rc", C, L);
        }

        // clang family

        // create programs
        const path base_llvm_path = "c:\\Program Files\\LLVM";
        const path bin_llvm_path = base_llvm_path / "bin";

        // clang-cl

        // C, C++
        {
            auto L = std::make_shared<NativeLanguage>();
            L->CompiledExtensions = getCppSourceFileExtensions();
            L->CompiledExtensions.insert(".c");

            auto C = std::make_shared<ClangClCompiler>();
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
            L->compiler = C;
            s.registerProgramAndLanguage("org.LLVM.clangcl", C, L);

            switch (s.Settings.TargetOS.Arch)
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

        /*auto Linker = std::make_shared<VisualStudioLinker>();
        Linker->Type = LinkerType::LLD;
        Linker->file = bin_llvm_path / "lld-link.exe";
        Linker->vs_version = VSVersion;
        *Linker = LOpts;

        auto Librarian = std::make_shared<VisualStudioLibrarian>();
        Librarian->Type = LinkerType::LLD;
        Librarian->file = bin_llvm_path / "llvm-ar.exe"; // ?
        Librarian->vs_version = VSVersion;
        *Librarian = LOpts;*/

        // C
        {
            auto L = std::make_shared<NativeLanguage>();
            L->CompiledExtensions = { ".c" };

            auto C = std::make_shared<ClangCompiler>();
            C->Type = CompilerType::Clang;
            C->file = bin_llvm_path / "clang.exe";
            C->PositionIndependentCode = false; // not available for msvc triple
            auto COpts2 = COpts;
            // is it able to find VC STL itself?
            //COpts2.System.IncludeDirectories.erase(root / "include");
            //COpts2.System.IncludeDirectories.erase(root / "ATLMFC\\include");
            COpts2.System.IncludeDirectories.insert(base_llvm_path / "lib" / "clang" / C->getVersion().toString() / "include");
            COpts2.System.CompileOptions.push_back("-Wno-everything");
            *C = COpts2;
            L->compiler = C;
            s.registerProgramAndLanguage("org.LLVM.clang", C, L);
        }

        // C++
        {
            auto L = std::make_shared<NativeLanguage>();
            L->CompiledExtensions = getCppSourceFileExtensions();

            auto C = std::make_shared<ClangCompiler>();
            C->Type = CompilerType::Clang;
            C->file = bin_llvm_path / "clang++.exe";
            C->PositionIndependentCode = false; // not available for msvc triple
            auto COpts2 = COpts;
            // is it able to find VC STL itself?
            //COpts2.System.IncludeDirectories.erase(root / "include");
            //COpts2.System.IncludeDirectories.erase(root / "ATLMFC\\include");
            COpts2.System.IncludeDirectories.insert(base_llvm_path / "lib" / "clang" / C->getVersion().toString() / "include");
            COpts2.System.CompileOptions.push_back("-Wno-everything");
            *C = COpts2;
            L->compiler = C;
            s.registerProgramAndLanguage("org.LLVM.clangpp", C, L);
        }
    };

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
            root /= "..\\..\\VC\\";
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

void detectNonWindowsCompilers(struct Solution &s)
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
        auto Librarian = std::make_shared<GNULibrarian>();
        Librarian->Type = LinkerType::GNU;
        Librarian->file = p;
        Librarian->Extension = s.Settings.TargetOS.getStaticLibraryExtension();
        *Librarian = LOpts;
        s.registerProgram("org.gnu.binutils.ar", Librarian);
        if (s.HostOS.is(OSType::Macos))
            Librarian->createCommand()->use_response_files = false;
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
    if (s.HostOS.is(OSType::Macos))
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
            auto Linker = std::make_shared<GNULinker>();

            if (s.HostOS.is(OSType::Macos))
                Linker->use_start_end_groups = false;
            Linker->Type = LinkerType::GNU;
            Linker->file = p;

            auto lopts2 = LOpts;
            //lopts2.System.LinkLibraries.push_back("stdc++"); // remove and add to progs explicitly?
            //lopts2.System.LinkLibraries.push_back("stdc++fs"); // remove and add to progs explicitly?

            *Linker = lopts2;
            s.registerProgram("org.gnu.gcc.ld", Linker);
        }
    }

    NativeCompilerOptions COpts;

    path macos_sdk_dir;
    if (s.Settings.TargetOS.is(OSType::Macos) || s.Settings.TargetOS.is(OSType::IOS))
        macos_sdk_dir = s.Settings.Native.SDK.getPath();

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
        s.registerProgramAndLanguage("org.gnu.gcc.as", C, L);
    }*/

    for (auto &v : gcc_vers)
    {
        p = resolve(v);
        if (!p.empty())
        {
            // C
            {
                auto L = std::make_shared<NativeLanguage>();
                // also with asm
                // .s - pure asm
                // .S - with #define (accepts -D) and #include (accepts -I), also .sx
                L->CompiledExtensions = { ".c", ".s", ".S" };

                auto C = std::make_shared<GNUCompiler>();
                C->Type = CompilerType::GNU;
                C->file = p;
                *C = COpts;
                L->compiler = C;
                s.registerProgramAndLanguage("org.gnu.gcc.gcc", C, L);

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
                auto L = std::make_shared<NativeLanguage>();
                L->CompiledExtensions = getCppSourceFileExtensions();

                auto C = std::make_shared<GNUCompiler>();
                C->Type = CompilerType::GNU;
                C->file = p;
                *C = COpts;
                L->compiler = C;
                s.registerProgramAndLanguage("org.gnu.gcc.gpp", C, L);

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
            auto Librarian = std::make_shared<GNULibrarian>();
            Librarian->Type = LinkerType::GNU;
            Librarian->file = p;
            Librarian->Extension = s.Settings.TargetOS.getStaticLibraryExtension();
            *Librarian = LOpts;
            s.registerProgram("org.LLVM.ar", Librarian);
            //if (s.HostOS.is(OSType::Macos))
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

                auto Linker = std::make_shared<GNULinker>();

                if (s.HostOS.is(OSType::Macos))
                    Linker->use_start_end_groups = false;
                Linker->Type = LinkerType::GNU;
                Linker->file = p;

                auto lopts2 = LOpts;
                //lopts2.System.LinkLibraries.push_back("c++"); // remove and add to progs explicitly?
                //lopts2.System.LinkLibraries.push_back("c++fs"); // remove and add to progs explicitly?

                *Linker = lopts2;
                s.registerProgram(appleclang ? "com.apple.LLVM.ld" : "org.LLVM.ld", Linker);

                if (s.HostOS.is(OSType::Macos))
                {
                    if (!appleclang)
                        Linker->GNULinkerOptions::LinkDirectories().push_back(p.parent_path().parent_path() / "lib");
                }

                NativeCompilerOptions COpts;

                // C
                {
                    auto L = std::make_shared<NativeLanguage>();
                    // also with asm
                    // .s - pure asm
                    // .S - with #define (accepts -D) and #include (accepts -I), also .sx
                    L->CompiledExtensions = { ".c", ".s", ".S" };

                    bool appleclang = is_apple_clang(p);

                    auto C = std::make_shared<ClangCompiler>();
                    C->Type = appleclang ? CompilerType::AppleClang : CompilerType::Clang;
                    C->file = p;
                    *C = COpts;
                    L->compiler = C;
                    s.registerProgramAndLanguage(appleclang ? "com.apple.LLVM.clang" : "org.LLVM.clang", C, L);

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
                    auto L = std::make_shared<NativeLanguage>();
                    L->CompiledExtensions = getCppSourceFileExtensions();

                    bool appleclang = is_apple_clang(p);

                    auto C = std::make_shared<ClangCompiler>();
                    C->Type = appleclang ? CompilerType::AppleClang : CompilerType::Clang;
                    C->file = p;
                    *C = COpts;
                    L->compiler = C;
                    s.registerProgramAndLanguage(appleclang ? "com.apple.LLVM.clangpp" : "org.LLVM.clangpp", C, L);

                    if (!macos_sdk_dir.empty())
                        C->IncludeSystemRoot = macos_sdk_dir;
                }
            }
        }
    }
}

void detectNativeCompilers(struct Solution &s)
{
    //auto &os = s.HostOS;
    auto &os = s.Settings.TargetOS;
    if (os.is(OSType::Windows) || os.is(OSType::Cygwin))
    {
        if (os.is(OSType::Cygwin))
            detectNonWindowsCompilers(s);
        detectWindowsCompilers(s);
    }
    else
        detectNonWindowsCompilers(s);
}

void VSInstance::activate(struct Solution &s) const
{
    //if (s.Settings.Native.CompilerType != CompilerType::MSVC)
        //throw SW_RUNTIME_ERROR("unsupported compiler " + toString(s.Settings.Native.CompilerType) + " for generator");

    if (cl_versions.empty())
        throw SW_RUNTIME_ERROR("missing cl.exe versions");
    if (link_versions.empty())
        throw SW_RUNTIME_ERROR("missing vs tools versions");

    if (!s.activateLanguage({ "com.Microsoft.VisualStudio.VC.cl", *cl_versions.rbegin() }, false))
        throw SW_RUNTIME_ERROR("cannot activate com.Microsoft.VisualStudio.VC.cl");
    if (!s.activateLanguage({ "com.Microsoft.VisualStudio.VC.ml", *link_versions.rbegin() }, false))
        throw SW_RUNTIME_ERROR("cannot activate com.Microsoft.VisualStudio.VC.ml");

    s.Settings.Native.CompilerType = CompilerType::MSVC;

    // linkers
    auto lib = s.getProgram({ "com.Microsoft.VisualStudio.VC.lib", *link_versions.rbegin() }, false);
    auto link = s.getProgram({ "com.Microsoft.VisualStudio.VC.link", *link_versions.rbegin() }, false);
    auto r = lib && link;
    if (r)
    {
        s.Settings.Native.Librarian = std::dynamic_pointer_cast<NativeLinker>(lib->clone());
        s.Settings.Native.Linker = std::dynamic_pointer_cast<NativeLinker>(link->clone());
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
    }
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

CompilerBaseProgram::CompilerBaseProgram(const CompilerBaseProgram &rhs)
    : Program(rhs)
{
    Extension = rhs.Extension;
    if (rhs.cmd)
        cmd = rhs.cmd->clone();
}

std::shared_ptr<builder::Command> CompilerBaseProgram::getCommand() const
{
    if (!cmd || !prepared)
        throw SW_RUNTIME_ERROR("Command is not prepared");
    return cmd;
}

std::shared_ptr<builder::Command> CompilerBaseProgram::createCommand()
{
    if (cmd)
        return cmd;
    return cmd = createCommand1();
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
    createCommand(); // do some init
    cmd->fs = t.getSolution()->fs;
    prepareCommand1(t);
    prepared = true;
    return cmd;
}

SW_CREATE_COMPILER_COMMAND(CompilerBaseProgram, SW_MAKE_COMPILER_COMMAND, driver::Command)

Strings NativeCompiler::getCStdOption(CLanguageStandard std) const
{
    String s = "-std=c";
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

Strings NativeCompiler::getClangCppStdOption(CPPLanguageStandard std) const
{
    String s = "-std=c++";
    switch (std)
    {
    case CPPLanguageStandard::CPP11:
        s += "11";
        break;
    case CPPLanguageStandard::CPP14:
        s += "14";
        break;
    case CPPLanguageStandard::CPP17:
        s += getVersion() > Version(5) ? "17" : "1z";
        break;
    case CPPLanguageStandard::CPPLatest:
        s += "2a";
        break;
    default:
        return {};
    }
    return { s };
}

Strings NativeCompiler::getGNUCppStdOption(CPPLanguageStandard std) const
{
    String s = "-std=c++";
    switch (std)
    {
    case CPPLanguageStandard::CPP11:
        s += "11";
        break;
    case CPPLanguageStandard::CPP14:
        s += "14";
        break;
    case CPPLanguageStandard::CPP17:
        s += getVersion() > Version(6) ? "17" : "1z";
        break;
    case CPPLanguageStandard::CPPLatest:
        s += "2a";
        break;
    default:
        return {};
    }
    return { s };
}

SW_CREATE_COMPILER_COMMAND(VisualStudioCompiler, SW_MAKE_COMPILER_COMMAND_WITH_FILE, driver::VSCommand)

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
    iterate([this](auto &v, auto &gs) { v.addEverything(*cmd); });

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

SW_CREATE_COMPILER_COMMAND(VisualStudioASMCompiler, SW_MAKE_COMPILER_COMMAND_WITH_FILE, driver::VSCommand)

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
    iterate([this](auto &v, auto &gs) { v.addEverything(*cmd); });
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

SW_CREATE_COMPILER_COMMAND(ClangCompiler, SW_MAKE_COMPILER_COMMAND_WITH_FILE, driver::GNUCommand)

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

    add_args(*cmd, getCStdOption(CStandard()));
    CStandard.skip = true;
    add_args(*cmd, getClangCppStdOption(CPPStandard()));
    CPPStandard.skip = true;

    getCommandLineOptions<ClangOptions>(cmd.get(), *this);
    iterate([this](auto &v, auto &gs) { v.addEverything(*this->cmd); });
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

SW_CREATE_COMPILER_COMMAND(ClangClCompiler, SW_MAKE_COMPILER_COMMAND_WITH_FILE, driver::VSCommand)

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

    add_args(*cmd, getClangCppStdOption(CPPStandard()));
    CPPStandard.skip = true;

    getCommandLineOptions<VisualStudioCompilerOptions>(cmd.get(), *this);
    getCommandLineOptions<ClangClOptions>(cmd.get(), *this/*, "-Xclang"*/);
    iterate([this](auto &v, auto &gs) { v.addEverything(*cmd); });
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

SW_CREATE_COMPILER_COMMAND(GNUASMCompiler, SW_MAKE_COMPILER_COMMAND_WITH_FILE, driver::GNUCommand)

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
        iterate([this](auto & v, auto & gs) { v.addEverything(*cmd); });
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

SW_CREATE_COMPILER_COMMAND(GNUCompiler, SW_MAKE_COMPILER_COMMAND_WITH_FILE, driver::GNUCommand)

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

    add_args(*cmd, getCStdOption(CStandard()));
    CStandard.skip = true;
    add_args(*cmd, getGNUCppStdOption(CPPStandard()));
    CPPStandard.skip = true;

    getCommandLineOptions<GNUOptions>(cmd.get(), *this);
    iterate([this](auto &v, auto &gs) { v.addEverything(*this->cmd); });
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
    iterate([&dirs](auto &v, auto &gs)
    {
        auto get_ldir = [&dirs](const auto &a)
        {
            for (auto &d : a)
                dirs.push_back(d);
        };

        get_ldir(v.gatherLinkDirectories());
        get_ldir(v.System.gatherLinkDirectories());
    });
    return dirs;
}

FilesOrdered NativeLinker::gatherLinkLibraries(bool system) const
{
    FilesOrdered dirs;
    iterate([&dirs, &system](auto &v, auto &gs)
    {
        auto get_ldir = [&dirs](const auto &a)
        {
            for (auto &d : a)
                dirs.push_back(d);
        };

        if (system)
            get_ldir(v.System.gatherLinkLibraries());
        else
            get_ldir(v.gatherLinkLibraries());
    });
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
    if (InputFiles.empty() && ModuleDefinitionFile.empty())
    {
        // why? maybe throw?
        cmd.reset();
        return;
    }

    if (Output.empty())
        throw SW_RUNTIME_ERROR("Output file is not set");

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
    iterate([this](auto &v, auto &gs) { v.addEverything(*cmd); });
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
    if (InputFiles.empty() && ModuleDefinitionFile.empty())
    {
        // why? maybe throw?
        cmd.reset();
        return;
    }

    if (Output.empty())
        throw SW_RUNTIME_ERROR("Output file is not set");

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
    iterate([this](auto &v, auto &gs) { v.addEverything(*cmd); });
    getAdditionalOptions(cmd.get());
}

SW_DEFINE_PROGRAM_CLONE(VisualStudioLibrarian)

void VisualStudioLibrarian::getAdditionalOptions(driver::Command *cmd) const
{
    getCommandLineOptions<VisualStudioLibrarianOptions>(cmd, *this);
}

SW_DEFINE_PROGRAM_CLONE(GNULinker)

void GNULinker::setObjectFiles(const Files &files)
{
    InputFiles().insert(files.begin(), files.end());
}

void GNULinker::setOutputFile(const path &out)
{
    Output = out.u8string() + Extension;
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
    if (InputFiles.empty()/* && ModuleDefinitionFile.empty()*/)
    {
        // why? maybe throw?
        cmd.reset();
        return;
    }

    if (Output.empty())
        throw SW_RUNTIME_ERROR("Output file is not set");

    // can be zero imput files actually: lib.exe /DEF:my.def /OUT:x.lib
    //if (InputFiles().empty())
        //return nullptr;

    //LinkDirectories() = gatherLinkDirectories();
    //((GNULinker*)this)->GNULinkerOptions::LinkLibraries() = gatherLinkLibraries();
    ((GNULinker*)this)->GNULinkerOptions::SystemLinkLibraries = gatherLinkLibraries(true);

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
    iterate([this](auto &v, auto &gs) { v.addEverything(*cmd); });
    //getAdditionalOptions(cmd.get());
}

SW_DEFINE_PROGRAM_CLONE(GNULibrarian)

void GNULibrarian::setObjectFiles(const Files &files)
{
    InputFiles().insert(files.begin(), files.end());
}

void GNULibrarian::setOutputFile(const path &out)
{
    Output = out.u8string() + Extension;
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
    if (InputFiles.empty()/* && ModuleDefinitionFile.empty()*/)
    {
        // why? maybe throw?
        cmd.reset();
        return;
    }

    if (Output.empty())
        throw SW_RUNTIME_ERROR("Output file is not set");

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
    iterate([this](auto &v, auto &gs) { v.addEverything(*cmd); });
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

    t.template asRef<NativeExecutedTarget>().NativeCompilerOptions::addDefinitionsAndIncludeDirectories(*cmd);

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
