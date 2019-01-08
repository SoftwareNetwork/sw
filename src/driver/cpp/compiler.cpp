// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <compiler.h>

#include <language.h>
#include <solution.h>
#include <compiler_helpers.h>

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
    SW_MAKE_COMPILER_COMMAND(driver::cpp::t)

static cl::opt<bool> do_not_resolve_compiler("do-not-resolve-compiler");
static cl::opt<bool> use_other_langs("use-other-languages");

extern const StringSet cpp_source_file_extensions;
extern const StringSet header_file_extensions;

const StringSet cpp_source_file_extensions{
	".cc",
	".CC",
	".cpp",
	".cxx",
	".ixx", // msvc modules
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

const StringSet header_file_extensions{
	".h",
	".hh",
	".hm",
	".hpp",
	".hxx",
	".h++",
	".H++",
	".HPP",
	".H",
};

namespace sw
{

void detectNativeCompilers(struct Solution &s);
void detectCSharpCompilers(struct Solution &s);
void detectRustCompilers(struct Solution &s);
void detectGoCompilers(struct Solution &s);
void detectFortranCompilers(struct Solution &s);
void detectJavaCompilers(struct Solution &s);
void detectKotlinCompilers(struct Solution &s);

static Version gatherVersion(const path &program, const String &arg = "--version", const String &in_regex = {})
{
    static std::regex r_default("(\\d+)\\.(\\d+)\\.(\\d+)(\\.(\\d+))?");

    std::regex r_in;
    if (!in_regex.empty())
        r_in.assign(in_regex);

    auto &r = in_regex.empty() ? r_default : r_in;

    Version V;
    primitives::Command c;
    c.program = program;
    c.args = { arg };
    error_code ec;
    c.execute(ec);
    std::smatch m;
    if (std::regex_search(c.err.text.empty() ? c.out.text : c.err.text, m, r))
    {
        if (m[5].matched)
            V = { std::stoi(m[1].str()), std::stoi(m[2].str()), std::stoi(m[3].str()), std::stoi(m[5].str()) };
        else
            V = { std::stoi(m[1].str()), std::stoi(m[2].str()), std::stoi(m[3].str()) };
    }
    return V;
}

static void add_args(driver::cpp::Command &c, const Strings &args)
{
    for (auto &a : args)
        c.args.push_back(a);
}

std::string getVsToolset(VisualStudioVersion v)
{
    switch (v)
    {
    case VisualStudioVersion::VS15:
        return "vc141";
    case VisualStudioVersion::VS14:
        return "vc14";
    case VisualStudioVersion::VS12:
        return "vc12";
    case VisualStudioVersion::VS11:
        return "vc11";
    case VisualStudioVersion::VS10:
        return "vc10";
    case VisualStudioVersion::VS9:
        return "vc9";
    case VisualStudioVersion::VS8:
        return "vc8";
    }
    throw SW_RUNTIME_ERROR("Unknown VS version");
}

path getProgramFilesX86()
{
    auto e = getenv("programfiles(x86)");
    if (!e)
        throw SW_RUNTIME_ERROR("Cannot get 'programfiles(x86)' env. var.");
    return e;
}

bool findDefaultVS(path &root, VisualStudioVersion &VSVersion)
{
    auto program_files_x86 = getProgramFilesX86();
    for (auto &edition : { "Enterprise", "Professional", "Community" })
    {
        for (const auto &[y, v] :
            std::vector<std::pair<String, VisualStudioVersion>>{ {"2017", VisualStudioVersion::VS15},
            {"2019", VisualStudioVersion::VS16} })
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

StringSet listMajorWindowsKits()
{
    StringSet kits;
    auto program_files_x86 = getProgramFilesX86();
    for (auto &k : { "10", "8.1", "8.0", "7.1A", "7.0A", "6.0A" })
    {
        auto d = program_files_x86 / "Windows Kits" / k;
        if (fs::exists(d))
            kits.insert(k);
    }
    return kits;
}

StringSet listWindows10Kits()
{
    StringSet kits;
    auto program_files_x86 = getProgramFilesX86();
    auto dir = program_files_x86 / "Windows Kits" / "10" / "Include";
    for (auto &i : fs::directory_iterator(dir))
    {
        if (fs::is_directory(i))
            kits.insert(i.path().filename().string());
    }
    return kits;
}

StringSet listWindowsKits()
{
    auto allkits = listMajorWindowsKits();
    auto i = allkits.find("10");
    if (i == allkits.end())
        return allkits;
    auto kits2 = listWindows10Kits();
    allkits.insert(kits2.begin(), kits2.end());
    return allkits;
}

String getLatestWindowsKit()
{
    auto allkits = listMajorWindowsKits();
    auto i = allkits.find("10");
    if (i == allkits.end())
        return *allkits.rbegin();
    return *listWindows10Kits().rbegin();
}

path getWindowsKitDir()
{
    auto program_files_x86 = getProgramFilesX86();
    for (auto &k : { "10", "8.1", "8.0", "7.1A", "7.0A", "6.0A" })
    {
        auto d = program_files_x86 / "Windows Kits" / k;
        if (fs::exists(d))
            return d;
    }
    throw SW_RUNTIME_ERROR("No Windows Kits available");
}

path getWindowsKit10Dir(Solution &s, const path &d)
{
    // take current or the latest version
    path last_dir = d / s.HostOS.Version.toString(true);
    if (fs::exists(last_dir))
        return last_dir;
    last_dir.clear();
    Version p;
    for (auto &i : fs::directory_iterator(d))
    {
        if (!fs::is_directory(i))
            continue;
        try
        {
            Version v(i.path().filename().u8string());
            if (v.isBranch())
                continue;
            if (v > p)
            {
                p = v;
                last_dir = i;
            }
        }
        catch (...)
        {
        }
    }
    if (last_dir.empty())
        throw SW_RUNTIME_ERROR("No Windows Kits 10.0 available");
    return last_dir;
}

void detectCompilers(struct Solution &s)
{
    detectNativeCompilers(s);

    // make lazy loading
    if (use_other_langs)
    {
        detectCSharpCompilers(s);
        detectRustCompilers(s);
        detectGoCompilers(s);
        detectFortranCompilers(s);
        detectJavaCompilers(s);
        detectKotlinCompilers(s);
    }
}

void detectKotlinCompilers(struct Solution &s)
{
    path compiler;
    compiler = primitives::resolve_executable("kotlinc");
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
    compiler = primitives::resolve_executable("javac");
    if (compiler.empty())
        return;
    //compiler = primitives::resolve_executable("jar"); // later

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
    compiler = primitives::resolve_executable("gfortran");
    if (compiler.empty())
    {
        compiler = primitives::resolve_executable("f95");
        if (compiler.empty())
        {
            compiler = primitives::resolve_executable("g95");
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
    L->compiler = C;
    s.registerProgramAndLanguage("org.gnu.gcc.fortran", C, L);
}

void detectGoCompilers(struct Solution &s)
{
#if defined(_WIN32)
    auto compiler = path("go");
    compiler = primitives::resolve_executable(compiler);
    if (compiler.empty())
        return;

    auto L = std::make_shared<GoLanguage>();
    L->CompiledExtensions = { ".go" };

    auto C = std::make_shared<GoCompiler>();
    C->file = compiler;
    L->compiler = C;
    s.registerProgramAndLanguage("org.google.golang.go", C, L);
#else
#endif
}

void detectRustCompilers(struct Solution &s)
{
#if defined(_WIN32)
    auto compiler = get_home_directory() / ".cargo" / "bin" / "rustc";
    compiler = primitives::resolve_executable(compiler);
    if (compiler.empty())
        return;

    auto L = std::make_shared<RustLanguage>();
    L->CompiledExtensions = { ".rs" };

    auto C = std::make_shared<RustCompiler>();
    C->file = compiler;
    L->compiler = C;
    s.registerProgramAndLanguage("org.rust.rustc", C, L);
#else
#endif
}

void detectCSharpCompilers(struct Solution &s)
{
    path root;
    auto VSVersion = VisualStudioVersion::Unspecified;

#if defined(_WIN32)
    cmVSSetupAPIHelper h;
    if (h.IsVSInstalled(15))
    {
        root = h.chosenInstanceInfo.VSInstallLocation;
        root = root / "MSBuild" / "15.0" / "Bin" / "Roslyn";
        VSVersion = VisualStudioVersion::VS15;
    }
    else if (h.IsVSInstalled(16))
    {
        root = h.chosenInstanceInfo.VSInstallLocation;
        root = root / "MSBuild" / "Current" / "Bin" / "Roslyn";
        VSVersion = VisualStudioVersion::VS16;
    }

    // we do not look for older compilers like vc7.1 and vc98
    if (VSVersion == VisualStudioVersion::Unspecified)
        return;

    auto compiler = root / "csc";

    auto L = std::make_shared<CSharpLanguage>();
    L->CompiledExtensions = { ".cs" };

    auto C = std::make_shared<VisualStudioCSharpCompiler>();
    C->file = compiler;
    L->compiler = C;
    s.registerProgramAndLanguage("com.Microsoft.VisualStudio.Roslyn.csc", C, L);
#endif
}

void detectNativeCompilers(struct Solution &s)
{
    //TODO: find preview versions also

    path root;
    Version V;
    auto VSVersion = VisualStudioVersion::Unspecified;

    auto find_comn_tools = [&root, &VSVersion](auto v)
    {
        auto n = std::to_string(v);
        auto ver = "VS"s + n + "COMNTOOLS";
        auto e = getenv(ver.c_str());
        if (e)
        {
            root = e;
            root /= "..\\..\\VC\\";
            VSVersion = v;
            return true;
        }
        return false;
    };

#if defined(_WIN32)
    cmVSSetupAPIHelper h;
    auto vs15 = h.IsVSInstalled(15);
    auto vs16 = h.IsVSInstalled(16);
    if (vs15 || vs16)
    {
        root = h.chosenInstanceInfo.VSInstallLocation;
        root /= "VC";
        VSVersion = vs15 ? VisualStudioVersion::VS15 : VisualStudioVersion::VS16;

        // can be split by points
        static std::wregex r(L"(\\d+)\\.(\\d+)\\.(\\d+)(\\.(\\d+))?");
        std::wsmatch m;
        if (!std::regex_match(h.chosenInstanceInfo.Version, m, r))
            throw SW_RUNTIME_ERROR("Cannot match vs version regex");
        if (m[5].matched)
            V = { std::stoi(m[1].str()), std::stoi(m[2].str()), std::stoi(m[3].str()), std::stoi(m[5].str()) };
        else
            V = { std::stoi(m[1].str()), std::stoi(m[2].str()), std::stoi(m[3].str()) };
    }
    else if (
        !find_comn_tools(VisualStudioVersion::VS16) &&
        !find_comn_tools(VisualStudioVersion::VS15) &&
        !findDefaultVS(root, VSVersion))
    {
        // find older versions
        static const auto vers =
        {
            VisualStudioVersion::VS14,
            VisualStudioVersion::VS12,
            VisualStudioVersion::VS11,
            VisualStudioVersion::VS10,
            VisualStudioVersion::VS9,
            VisualStudioVersion::VS8,
        };
        for (auto n : vers)
        {
            if (find_comn_tools(n))
                break;
        }
    }

    // we do not look for older compilers like vc7.1 and vc98
    if (VSVersion == VisualStudioVersion::Unspecified)
        return;

    if (VSVersion >= VisualStudioVersion::VS15)
        root = root / "Tools\\MSVC" / boost::trim_copy(read_file(root / "Auxiliary\\Build\\Microsoft.VCToolsVersion.default.txt"));

    auto ToolSet = getVsToolset(VSVersion);
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
    switch (s.HostOS.Arch)
    {
    case ArchType::x86_64:
        dir_suffix.host = "x64";
        break;
    case ArchType::x86:
        dir_suffix.host = "x86";
        break;
        // arm
        //dir_suffix.include = "arm";
        //dir_suffix.lib = "arm";
        // arm64 !
        //dir_suffix.include = "arm";
        //dir_suffix.lib = "arm64";
    default:
        assert(false && "Unknown arch");
    }

    switch (s.Settings.TargetOS.Arch)
    {
    case ArchType::x86_64:
        dir_suffix.target = "x64";
        break;
    case ArchType::x86:
        dir_suffix.host = "x86";
        dir_suffix.target = "x86";
        break;
        // arm
        //dir_suffix.include = "arm";
        //dir_suffix.lib = "arm";
        // arm64 !
        //dir_suffix.include = "arm";
        //dir_suffix.lib = "arm64";
    default:
        assert(false && "Unknown arch");
    }

    NativeLinkerOptions LOpts;

    // continue
    if (VSVersion >= VisualStudioVersion::VS15)
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
    auto windows_kit_dir = getWindowsKitDir();
    for (auto &i : fs::directory_iterator(getWindowsKit10Dir(s, windows_kit_dir / "include")))
    {
        if (fs::is_directory(i))
            COpts.System.IncludeDirectories.insert(i);
    }
    for (auto &i : fs::directory_iterator(getWindowsKit10Dir(s, windows_kit_dir / "lib")))
    {
        if (fs::is_directory(i))
            LOpts.System.LinkDirectories.insert(i / path(dir_suffix.target));
    }

    // create programs

    {
        auto Linker = std::make_shared<VisualStudioLinker>();
        Linker->Type = LinkerType::MSVC;
        Linker->file = compiler.parent_path() / "link.exe";
        Linker->vs_version = VSVersion;
        if (s.Settings.TargetOS.Arch == ArchType::x86)
            Linker->Machine = vs::MachineType::X86;
        *Linker = LOpts;
        s.registerProgram("com.Microsoft.VisualStudio.VC.link", Linker);

        auto Librarian = std::make_shared<VisualStudioLibrarian>();
        Librarian->Type = LinkerType::MSVC;
        Librarian->file = compiler.parent_path() / "lib.exe";
        Librarian->vs_version = VSVersion;
        if (s.Settings.TargetOS.Arch == ArchType::x86)
            Librarian->Machine = vs::MachineType::X86;
        *Librarian = LOpts;
        s.registerProgram("com.Microsoft.VisualStudio.VC.lib", Librarian);
    }

    // ASM
    {
        auto L = std::make_shared<NativeLanguage>();
        //L->Type = LanguageType::ASM;
        L->CompiledExtensions = { ".asm" };
        //s.registerLanguage(L);

        //auto L = (ASMLanguage*)s.languages[LanguageType::ASM].get();
        auto C = std::make_shared<VisualStudioASMCompiler>();
        C->Type = CompilerType::MSVC;
        C->file = s.HostOS.Arch == ArchType::x86_64 ?
            (compiler.parent_path() / "ml64.exe") :
            (compiler.parent_path() / "ml.exe");
        C->vs_version = VSVersion;
        *C = COpts;
        L->compiler = C;
        s.registerProgramAndLanguage("com.Microsoft.VisualStudio.VC.ml", C, L);
    }

    // C
    {
        auto L = std::make_shared<NativeLanguage>();
        //L->Type = LanguageType::C;
        L->CompiledExtensions = { ".c" };
        //s.registerLanguage(L);

        //auto L = (CLanguage*)s.languages[LanguageType::C].get();
        auto C = std::make_shared<VisualStudioCompiler>();
        C->Type = CompilerType::MSVC;
        C->file = compiler;
        C->vs_version = VSVersion;
        *C = COpts;
        L->compiler = C;
        s.registerProgramAndLanguage("com.Microsoft.VisualStudio.VC.cl", C, L);
    }

    // C++
    {
        auto L = std::make_shared<NativeLanguage>();
        //L->Type = LanguageType::C;
        L->CompiledExtensions = cpp_source_file_extensions;
        //s.registerLanguage(L);

        //auto L = (CLanguage*)s.languages[LanguageType::C].get();
        auto C = std::make_shared<VisualStudioCompiler>();
        C->Type = CompilerType::MSVC;
        C->file = compiler;
        C->vs_version = VSVersion;
        *C = COpts;
        L->compiler = C;
        C->CompileAsCPP = true;
        s.registerProgramAndLanguage("com.Microsoft.VisualStudio.VC.clpp", C, L);
    }

    // .rc
    {
        auto L = std::make_shared<RcToolLanguage>();
        L->CompiledExtensions = { ".rc" };

        auto C = std::make_shared<RcTool>();
        C->file = getWindowsKit10Dir(s, windows_kit_dir / "bin") / dir_suffix.host / "rc.exe";
        for (auto &idir : COpts.System.IncludeDirectories)
            C->system_idirs.push_back(idir);

        L->compiler = C;
        s.registerProgramAndLanguage("com.Microsoft.VisualStudio.VC.rc", C, L);
    }

    // clang

    // create programs
    const path base_llvm_path = "c:\\Program Files\\LLVM";
    const path bin_llvm_path = base_llvm_path / "bin";

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
        //L->Type = LanguageType::C;
        L->CompiledExtensions = { ".c" };
        //s.registerLanguage(L);

        //auto L = (CLanguage*)s.languages[LanguageType::C].get();
        auto C = std::make_shared<ClangCompiler>();
        C->Type = CompilerType::Clang;
        C->file = bin_llvm_path / "clang.exe";
        auto COpts2 = COpts;
        COpts2.System.IncludeDirectories.erase(root / "include");
        COpts2.System.IncludeDirectories.erase(root / "ATLMFC\\include"); // also add
        COpts2.System.IncludeDirectories.insert(base_llvm_path / "lib" / "clang" / C->getVersion().toString() / "include");
        COpts2.System.CompileOptions.push_back("-Wno-everything");
        *C = COpts2;
        L->compiler = C;
        s.registerProgramAndLanguage("org.LLVM.clang", C, L);
    }

    // C++
    {
        auto L = std::make_shared<NativeLanguage>();
        //L->Type = LanguageType::C;
        L->CompiledExtensions = cpp_source_file_extensions;
        //s.registerLanguage(L);

        //auto L = (CLanguage*)s.languages[LanguageType::C].get();
        auto C = std::make_shared<ClangCompiler>();
        C->Type = CompilerType::Clang;
        C->file = bin_llvm_path / "clang++.exe";
        auto COpts2 = COpts;
        COpts2.System.IncludeDirectories.erase(root / "include");
        COpts2.System.IncludeDirectories.erase(root / "ATLMFC\\include"); // also add
        COpts2.System.IncludeDirectories.insert(base_llvm_path / "lib" / "clang" / C->getVersion().toString() / "include");
        COpts2.System.CompileOptions.push_back("-Wno-everything");
        *C = COpts2;
        L->compiler = C;
        s.registerProgramAndLanguage("org.LLVM.clangpp", C, L);
    }

    // clang-cl

    // C
    {
        auto L = std::make_shared<NativeLanguage>();
        //L->Type = LanguageType::C;
		L->CompiledExtensions = cpp_source_file_extensions;
		L->CompiledExtensions.insert(".c");
        //s.registerLanguage(L);

        //auto L = (CLanguage*)s.languages[LanguageType::C].get();
        auto C = std::make_shared<ClangClCompiler>();
        C->Type = CompilerType::ClangCl;
        C->file = bin_llvm_path / "clang-cl.exe";
        auto COpts2 = COpts;
        COpts2.System.IncludeDirectories.erase(root / "include");
        COpts2.System.IncludeDirectories.erase(root / "ATLMFC\\include"); // also add
        COpts2.System.IncludeDirectories.insert(bin_llvm_path / "lib" / "clang" / C->getVersion().toString() / "include");
        COpts2.System.CompileOptions.push_back("-Wno-everything");
        *C = COpts2;
        L->compiler = C;
        s.registerProgramAndLanguage("org.LLVM.clangcl", C, L);
    }
#else
    // gnu

    path p;

    NativeLinkerOptions LOpts;
    LOpts.System.LinkDirectories.insert("/lib");
    LOpts.System.LinkDirectories.insert("/lib/x86_64-linux-gnu");
    LOpts.System.LinkLibraries.push_back("stdc++");
    LOpts.System.LinkLibraries.push_back("stdc++fs");
    LOpts.System.LinkLibraries.push_back("pthread");
    LOpts.System.LinkLibraries.push_back("dl");
    LOpts.System.LinkLibraries.push_back("m");

    auto resolve = [](const path &p)
    {
        if (do_not_resolve_compiler)
            return p;
        return primitives::resolve_executable(p);
    };

    p = resolve("ar");
    if (!p.empty())
    {
        auto Librarian = std::make_shared<GNULibrarian>();
        Librarian->Type = LinkerType::GNU;
        Librarian->file = p;
        *Librarian = LOpts;
        s.registerProgram("org.gnu.binutils.ar", Librarian);
    }

    Strings gcc_vers{ "gcc" };
    Strings gccpp_vers{ "g++" };
    for (int i = 4; i < 12; i++)
    {
        gcc_vers.push_back(gcc_vers[0] + "-" + std::to_string(i));
        gccpp_vers.push_back(gccpp_vers[0] + "-" + std::to_string(i));
    }
    Strings clang_vers{ "clang" };
    Strings clangpp_vers{ "clang++" };
    for (int i = 3; i < 16; i++)
    {
        clang_vers.push_back(clang_vers[0] + "-" + std::to_string(i));
        clangpp_vers.push_back(clangpp_vers[0] + "-" + std::to_string(i));
    }

    //p = resolve("ld.gold");
    for (auto &v : gcc_vers)
    {
        p = resolve(v);
        if (!p.empty())
        {
            auto Linker = std::make_shared<GNULinker>();

            Linker->Type = LinkerType::GNU;
            Linker->file = p;
            *Linker = LOpts;
            s.registerProgram("org.gnu.gcc.ld", Linker);
        }
    }

    NativeCompilerOptions COpts;
    //COpts.System.IncludeDirectories.insert("/usr/include");
    //COpts.System.IncludeDirectories.insert("/usr/include/x86_64-linux-gnu");

    // ASM
    {
        p = resolve("as");

        auto L = std::make_shared<NativeLanguage>();
        //L->Type = LanguageType::ASM;
        L->CompiledExtensions = { ".s", ".S" };
        //s.registerLanguage(L);

        //auto L = (ASMLanguage*)s.languages[LanguageType::ASM].get();
        auto C = std::make_shared<GNUASMCompiler>();
        C->Type = CompilerType::GNU;
        C->file = p;
        *C = COpts;
        L->compiler = C;
        s.registerProgramAndLanguage("org.gnu.gcc.as", C, L);
    }

    for (auto &v : gcc_vers)
    {
        p = resolve(v);
        if (!p.empty())
        {
            // C
            {
                auto L = std::make_shared<NativeLanguage>();
                //L->Type = LanguageType::C;
                L->CompiledExtensions = { ".c" };
                //s.registerLanguage(L);

                //auto L = (CLanguage*)s.languages[LanguageType::C].get();
                auto C = std::make_shared<GNUCompiler>();
                C->Type = CompilerType::GNU;
                C->file = p;
                *C = COpts;
                L->compiler = C;
                s.registerProgramAndLanguage("org.gnu.gcc.gcc", C, L);
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
                //L->Type = LanguageType::C;
				L->CompiledExtensions = cpp_source_file_extensions;
                //s.registerLanguage(L);

                //auto L = (CPPLanguage*)s.languages[LanguageType::CPP].get();
                auto C = std::make_shared<GNUCompiler>();
                C->Type = CompilerType::GNU;
                C->file = p;
                *C = COpts;
                L->compiler = C;
                s.registerProgramAndLanguage("org.gnu.gcc.gpp", C, L);
            }
        }
    }

    // clang
    {
        //p = resolve("ld.gold");
        for (auto &v : clang_vers)
        {
            p = resolve(v);
            if (!p.empty())
            {
                auto Linker = std::make_shared<GNULinker>();

                Linker->Type = LinkerType::GNU;
                Linker->file = p;
                *Linker = LOpts;
                s.registerProgram("org.LLVM.clang.ld", Linker);

                NativeCompilerOptions COpts;
                //COpts.System.IncludeDirectories.insert("/usr/include");
                //COpts.System.IncludeDirectories.insert("/usr/include/x86_64-linux-gnu");

                // C
                {
                    auto L = std::make_shared<NativeLanguage>();
                    //L->Type = LanguageType::C;
                    L->CompiledExtensions = { ".c" };
                    //s.registerLanguage(L);

                    //auto L = (CLanguage*)s.languages[LanguageType::C].get();
                    auto C = std::make_shared<GNUCompiler>();
                    C->Type = CompilerType::Clang;
                    C->file = p;
                    *C = COpts;
                    L->compiler = C;
                    s.registerProgramAndLanguage("org.LLVM.clang", C, L);
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
                    //L->Type = LanguageType::C;
					L->CompiledExtensions = cpp_source_file_extensions;
                    //s.registerLanguage(L);

                    //auto L = (CPPLanguage*)s.languages[LanguageType::CPP].get();
                    auto C = std::make_shared<GNUCompiler>();
                    C->Type = CompilerType::Clang;
                    C->file = p;
                    *C = COpts;
                    L->compiler = C;
                    s.registerProgramAndLanguage("org.LLVM.clangpp", C, L);
                }
            }
        }
    }
#endif
}

std::shared_ptr<builder::Command> CompilerBaseProgram::getCommand() const
{
    if (!cmd)
        throw SW_RUNTIME_ERROR("Command is not prepared");
    return cmd;
}

std::shared_ptr<builder::Command> CompilerBaseProgram::getCommand(const TargetBase &t)
{
    prepareCommand(t);
    return getCommand();
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
        s += getVersion() > 5 ? "17" : "1z";
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
        s += getVersion() > 6 ? "17" : "1z";
        break;
    case CPPLanguageStandard::CPPLatest:
        s += "2a";
        break;
    default:
        return {};
    }
    return { s };
}

Version MsProgram::gatherVersion(const path &program) const
{
    return ::sw::gatherVersion(program, "/?");
}

std::shared_ptr<builder::Command> VisualStudioCompiler::prepareCommand(const TargetBase &t)
{
    if (cmd)
        return cmd;

    SW_MAKE_COMPILER_COMMAND_WITH_FILE(VSCommand);

    if (InputFile)
    {
        c->name = normalize_path(InputFile());
        c->name_short = InputFile().filename().u8string();
        //c->file = InputFile;
    }
    if (CSourceFile)
    {
        c->name = normalize_path(CSourceFile());
        c->name_short = CSourceFile().filename().u8string();
        //c->file = CSourceFile;
    }
    if (CPPSourceFile)
    {
        c->name = normalize_path(CPPSourceFile());
        c->name_short = CPPSourceFile().filename().u8string();
        //c->file = CPPSourceFile;
    }
    if (ObjectFile)
        c->working_directory = ObjectFile().parent_path();

    //if (c->file.empty())
        //return nullptr;

    //c->out.capture = true;

    getCommandLineOptions<VisualStudioCompilerOptions>(c.get(), *this);
    iterate([c](auto &v, auto &gs) { v.addEverything(*c); });

    if (PreprocessToFile)
    {
        //c->addOutput(c->file.file.parent_path() / (c->file.file.filename().stem().u8string() + ".i"));
        // TODO: remove old object file, it's now incorrect
    }

    return cmd = c;
}

void VisualStudioCompiler::setOutputFile(const path &output_file)
{
    ObjectFile = output_file;
}

SW_DEFINE_PROGRAM_CLONE(VisualStudioCompiler)

void VisualStudioCompiler::setSourceFile(const path &input_file, path &output_file)
{
    InputFile = input_file.u8string();
    VisualStudioCompiler::setOutputFile(output_file);
}

std::shared_ptr<builder::Command> VisualStudioASMCompiler::prepareCommand(const TargetBase &t)
{
    if (cmd)
        return cmd;

    SW_MAKE_COMPILER_COMMAND_WITH_FILE(VSCommand);

    if (file.filename() == "ml64.exe")
        ((VisualStudioASMCompiler*)this)->SafeSEH = false;

    if (InputFile)
    {
        c->name = normalize_path(InputFile());
        c->name_short = InputFile().filename().u8string();
        //c->file = InputFile;
    }
    if (ObjectFile)
        c->working_directory = ObjectFile().parent_path();

    //if (c->file.empty())
        //return nullptr;

    //c->out.capture = true;
    //c->base = clone();

    getCommandLineOptions<VisualStudioAssemblerOptions>(c.get(), *this);
    iterate([c](auto &v, auto &gs) { v.addEverything(*c); });

    return cmd = c;
}

SW_DEFINE_PROGRAM_CLONE(VisualStudioASMCompiler)

void VisualStudioASMCompiler::setOutputFile(const path &output_file)
{
    ObjectFile = output_file;
}

void VisualStudioASMCompiler::setSourceFile(const path &input_file, path &output_file)
{
    InputFile = input_file.u8string();
    setOutputFile(output_file);
}

Version Clang::gatherVersion(const path &program) const
{
    return ::sw::gatherVersion(program, "-v");
}

std::shared_ptr<builder::Command> ClangCompiler::prepareCommand(const TargetBase &t)
{
    if (cmd)
        return cmd;

    SW_MAKE_COMPILER_COMMAND_WITH_FILE(GNUCommand);

    if (InputFile)
    {
        c->name = normalize_path(InputFile());
        c->name_short = InputFile().filename().u8string();
        //c->file = InputFile;
    }
    if (OutputFile)
    {
        c->deps_file = OutputFile().parent_path() / (OutputFile().stem().u8string() + ".d");
        c->working_directory = OutputFile().parent_path();
    }

    //if (c->file.empty())
        //return nullptr;

    //c->out.capture = true;
    //c->base = clone();

    add_args(*c, getClangCppStdOption(CPPStandard()));
    CPPStandard.skip = true;

    getCommandLineOptions<ClangOptions>(c.get(), *this);
    iterate([c](auto &v, auto &gs) { v.addEverything(*c); });

    return cmd = c;
}

void ClangCompiler::setOutputFile(const path &output_file)
{
    OutputFile = output_file;
}

SW_DEFINE_PROGRAM_CLONE(ClangCompiler)

void ClangCompiler::setSourceFile(const path &input_file, path &output_file)
{
    InputFile = input_file.u8string();
    setOutputFile(output_file);
}

std::shared_ptr<builder::Command> ClangClCompiler::prepareCommand(const TargetBase &t)
{
    if (cmd)
        return cmd;

    SW_MAKE_COMPILER_COMMAND_WITH_FILE(VSCommand);

    if (InputFile)
    {
        c->name = normalize_path(InputFile());
        c->name_short = InputFile().filename().u8string();
        //c->file = InputFile;
    }
    if (CSourceFile)
    {
        c->name = normalize_path(CSourceFile());
        c->name_short = CSourceFile().filename().u8string();
        //c->file = CSourceFile;
    }
    if (CPPSourceFile)
    {
        c->name = normalize_path(CPPSourceFile());
        c->name_short = CPPSourceFile().filename().u8string();
        //c->file = CPPSourceFile;
    }
    if (ObjectFile)
        c->working_directory = ObjectFile().parent_path();

    //if (c->file.empty())
        //return nullptr;

    //c->out.capture = true;
    //c->base = clone();

    add_args(*c, getClangCppStdOption(CPPStandard()));
    CPPStandard.skip = true;

    getCommandLineOptions<VisualStudioCompilerOptions>(c.get(), *this);
    getCommandLineOptions<ClangClOptions>(c.get(), *this, "-Xclang");
    iterate([c](auto &v, auto &gs) { v.addEverything(*c); });

    return cmd = c;
}

void ClangClCompiler::setOutputFile(const path &output_file)
{
    ObjectFile = output_file;
}

SW_DEFINE_PROGRAM_CLONE(ClangClCompiler)

void ClangClCompiler::setSourceFile(const path &input_file, path &output_file)
{
    InputFile = input_file.u8string();
    setOutputFile(output_file);
}

Version GNU::gatherVersion(const path &program) const
{
    return ::sw::gatherVersion(program, "-v");
}

std::shared_ptr<builder::Command> GNUASMCompiler::prepareCommand(const TargetBase &t)
{
    if (cmd)
        return cmd;

    SW_MAKE_COMPILER_COMMAND_WITH_FILE(GNUCommand);

    if (InputFile)
    {
        c->name = normalize_path(InputFile());
        c->name_short = InputFile().filename().u8string();
        //c->file = InputFile;
    }
    if (OutputFile)
        c->working_directory = OutputFile().parent_path();

    //if (c->file.empty())
        //return nullptr;

    //c->out.capture = true;

    getCommandLineOptions<GNUAssemblerOptions>(c.get(), *this);
    iterate([c](auto &v, auto &gs) { v.addEverything(*c); });

    return cmd = c;
}

SW_DEFINE_PROGRAM_CLONE(GNUASMCompiler)

void GNUASMCompiler::setOutputFile(const path &output_file)
{
    OutputFile = output_file;
}

void GNUASMCompiler::setSourceFile(const path &input_file, path &output_file)
{
    InputFile = input_file.u8string();
    setOutputFile(output_file);
}

SW_DEFINE_PROGRAM_CLONE(ClangASMCompiler)

std::shared_ptr<builder::Command> GNUCompiler::prepareCommand(const TargetBase &t)
{
    if (cmd)
        return cmd;

    SW_MAKE_COMPILER_COMMAND_WITH_FILE(GNUCommand);

    if (InputFile)
    {
        c->name = normalize_path(InputFile());
        c->name_short = InputFile().filename().u8string();
        //c->file = InputFile;
    }
    if (OutputFile)
    {
        c->deps_file = OutputFile().parent_path() / (OutputFile().stem().u8string() + ".d");
        c->working_directory = OutputFile().parent_path();
    }

    //if (c->file.empty())
        //return nullptr;

    //c->out.capture = true;

    add_args(*c, getGNUCppStdOption(CPPStandard()));
    CPPStandard.skip = true;

    getCommandLineOptions<GNUOptions>(c.get(), *this);
    iterate([c](auto &v, auto &gs) { v.addEverything(*c); });
    getCommandLineOptions<GNUOptions>(c.get(), *this, "", true);

    return cmd = c;
}

void GNUCompiler::setOutputFile(const path &output_file)
{
    OutputFile = output_file;
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

        get_ldir(v.System.gatherLinkDirectories());
        get_ldir(v.gatherLinkDirectories());
    });
    return dirs;
}

FilesOrdered NativeLinker::gatherLinkLibraries() const
{
    FilesOrdered dirs;
    iterate([&dirs](auto &v, auto &gs)
    {
        auto get_ldir = [&dirs](const auto &a)
        {
            for (auto &d : a)
                dirs.push_back(d);
        };

        get_ldir(v.System.gatherLinkLibraries());
        get_ldir(v.gatherLinkLibraries());
    });
    return dirs;
}

void VisualStudioLibraryTool::setObjectFiles(const Files &files)
{
    if (!files.empty())
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

std::shared_ptr<builder::Command> VisualStudioLibraryTool::prepareCommand(const TargetBase &t)
{
    if (cmd)
        return cmd;

    if (InputFiles.empty() && DefinitionFile.empty())
        return nullptr;

    if (Output.empty())
        throw SW_RUNTIME_ERROR("Output file is not set");

    // can be zero imput files actually: lib.exe /DEF:my.def /OUT:x.lib
    //if (InputFiles().empty())
        //return nullptr;

    //LinkDirectories() = gatherLinkDirectories();
    //LinkLibraries() = gatherLinkLibraries();

    SW_MAKE_COMPILER_COMMAND(driver::cpp::Command);

    //c->out.capture = true;
    //c->base = clone();
    if (Output)
    {
        c->working_directory = Output().parent_path();
        c->name = normalize_path(Output());
        c->name_short = Output().filename().u8string();
    }

    /*if (c->name.find("eccdata.exe") != -1)
    {
        int a = 5;
        a++;
    }*/

    ((VisualStudioLibraryTool*)this)->VisualStudioLibraryToolOptions::LinkDirectories() = gatherLinkDirectories();

    getCommandLineOptions<VisualStudioLibraryToolOptions>(c.get(), *this);
    iterate([c](auto &v, auto &gs) { v.addEverything(*c); });
    getAdditionalOptions(c.get());

    return cmd = c;
}

VisualStudioLinker::VisualStudioLinker()
{
    Extension = ".exe";
}

SW_DEFINE_PROGRAM_CLONE(VisualStudioLinker)

void VisualStudioLinker::getAdditionalOptions(driver::cpp::Command *c) const
{
    getCommandLineOptions<VisualStudioLinkerOptions>(c, *this);
}

void VisualStudioLinker::setInputLibraryDependencies(const FilesOrdered &files)
{
    if (!files.empty())
        InputLibraryDependencies().insert(InputLibraryDependencies().end(), files.begin(), files.end());
}

VisualStudioLibrarian::VisualStudioLibrarian()
{
    Extension = ".lib";
}

SW_DEFINE_PROGRAM_CLONE(VisualStudioLibrarian)

void VisualStudioLibrarian::getAdditionalOptions(driver::cpp::Command *c) const
{
    getCommandLineOptions<VisualStudioLibrarianOptions>(c, *this);
}

GNULinker::GNULinker()
{
    //Extension = ".exe";
}

SW_DEFINE_PROGRAM_CLONE(GNULinker)

void GNULinker::setObjectFiles(const Files &files)
{
    if (!files.empty())
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
    InputLibraryDependencies().push_back("-Wl,--start-group");
    InputLibraryDependencies().insert(InputLibraryDependencies().end(), files.begin(), files.end());
    InputLibraryDependencies().push_back("-Wl,--end-group");
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

void GNULinker::getAdditionalOptions(driver::cpp::Command *c) const
{
    getCommandLineOptions<GNULinkerOptions>(c, *this);
}

std::shared_ptr<builder::Command> GNULinker::prepareCommand(const TargetBase &t)
{
    if (cmd)
        return cmd;

    if (InputFiles.empty()/* && DefinitionFile.empty()*/)
        return nullptr;

    if (Output.empty())
        throw SW_RUNTIME_ERROR("Output file is not set");

    // can be zero imput files actually: lib.exe /DEF:my.def /OUT:x.lib
    //if (InputFiles().empty())
        //return nullptr;

    //LinkDirectories() = gatherLinkDirectories();
    ((GNULinker*)this)->GNULinkerOptions::LinkLibraries() = gatherLinkLibraries();

    SW_MAKE_COMPILER_COMMAND(driver::cpp::Command);

    //c->out.capture = true;
    //c->base = clone();
    if (Output)
    {
        c->working_directory = Output().parent_path();
        c->name = normalize_path(Output());
        c->name_short = Output().filename().u8string();
    }

    /*if (c->name.find("eccdata.exe") != -1)
    {
        int a = 5;
        a++;
    }*/

    //((GNULibraryTool*)this)->GNULibraryToolOptions::LinkDirectories() = gatherLinkDirectories();

    getCommandLineOptions<GNULinkerOptions>(c.get(), *this);
    iterate([c](auto &v, auto &gs) { v.addEverything(*c); });
    //getAdditionalOptions(c.get());

    return cmd = c;
}

GNULibrarian::GNULibrarian()
{
    Extension = ".a";
}

SW_DEFINE_PROGRAM_CLONE(GNULibrarian)

void GNULibrarian::setObjectFiles(const Files &files)
{
    if (!files.empty())
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

void GNULibrarian::getAdditionalOptions(driver::cpp::Command *c) const
{
    getCommandLineOptions<GNULibrarianOptions>(c, *this);
}

std::shared_ptr<builder::Command> GNULibrarian::prepareCommand(const TargetBase &t)
{
    if (cmd)
        return cmd;

    if (InputFiles.empty()/* && DefinitionFile.empty()*/)
        return nullptr;

    if (Output.empty())
        throw SW_RUNTIME_ERROR("Output file is not set");

    // can be zero imput files actually: lib.exe /DEF:my.def /OUT:x.lib
    //if (InputFiles().empty())
        //return nullptr;

    //LinkDirectories() = gatherLinkDirectories();
    //LinkLibraries() = gatherLinkLibraries();

    SW_MAKE_COMPILER_COMMAND(driver::cpp::Command);

    //c->out.capture = true;
    //c->base = clone();
    if (Output)
    {
        c->working_directory = Output().parent_path();
        c->name = normalize_path(Output());
        c->name_short = Output().filename().u8string();
    }

    /*if (c->name.find("eccdata.exe") != -1)
    {
        int a = 5;
        a++;
    }*/

    //((GNULibraryTool*)this)->GNULibraryToolOptions::LinkDirectories() = gatherLinkDirectories();

    getCommandLineOptions<GNULibrarianOptions>(c.get(), *this);
    iterate([c](auto &v, auto &gs) { v.addEverything(*c); });
    //getAdditionalOptions(c.get());

    return cmd = c;
}

SW_DEFINE_PROGRAM_CLONE(RcTool)

std::shared_ptr<builder::Command> RcTool::prepareCommand(const TargetBase &t)
{
    if (cmd)
        return cmd;

    SW_MAKE_COMPILER_COMMAND(driver::cpp::Command);

    c->protect_args_with_quotes = false;

    if (InputFile)
    {
        c->name = normalize_path(InputFile());
        c->name_short = InputFile().filename().u8string();
    }

    t.template asRef<NativeExecutedTarget>().NativeCompilerOptions::addDefinitionsAndIncludeDirectories(*c);

    // ms bug: https://developercommunity.visualstudio.com/content/problem/417189/rcexe-incorrect-behavior-with.html
    //for (auto &i : system_idirs)
        //c->args.push_back("-I" + normalize_path(i));

    // use env
    String s;
    for (auto &i : system_idirs)
        s += normalize_path(i) + ";";
    c->environment["INCLUDE"] = s;

    // fix spaces around defs value:
    // from: -DSW_PACKAGE_API=extern \"C\" __declspec(dllexport)
    // to:   -DSW_PACKAGE_API="extern \"C\" __declspec(dllexport)"

    // find better way - protect things in addEverything?

    for (auto &a : c->args)
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

    getCommandLineOptions<RcToolOptions>(c.get(), *this);

    return cmd = c;
}

void RcTool::setOutputFile(const path &output_file)
{
    OutputFile = output_file;
}

void RcTool::setSourceFile(const path &input_file)
{
    InputFile = input_file;
}

SW_DEFINE_PROGRAM_CLONE(VisualStudioCSharpCompiler)

std::shared_ptr<builder::Command> VisualStudioCSharpCompiler::prepareCommand(const TargetBase &t)
{
    if (cmd)
        return cmd;

    SW_MAKE_COMPILER_COMMAND(driver::cpp::Command);

    getCommandLineOptions<VisualStudioCSharpCompilerOptions>(c.get(), *this);

    return cmd = c;
}

void VisualStudioCSharpCompiler::setOutputFile(const path &output_file)
{
    Output = output_file;
    Output() += ".exe";
}

void VisualStudioCSharpCompiler::addSourceFile(const path &input_file)
{
    InputFiles().insert(input_file);
}

SW_DEFINE_PROGRAM_CLONE(RustCompiler)

std::shared_ptr<builder::Command> RustCompiler::prepareCommand(const TargetBase &t)
{
    if (cmd)
        return cmd;

    SW_MAKE_COMPILER_COMMAND(driver::cpp::Command);

    getCommandLineOptions<RustCompilerOptions>(c.get(), *this);

    return cmd = c;
}

void RustCompiler::setOutputFile(const path &output_file)
{
    Output = output_file;
    Output() += ".exe";
}

void RustCompiler::setSourceFile(const path &input_file)
{
    InputFile() = input_file;
}

Version RustCompiler::gatherVersion() const
{
    return ::sw::gatherVersion(file);
}

SW_DEFINE_PROGRAM_CLONE(GoCompiler)

std::shared_ptr<builder::Command> GoCompiler::prepareCommand(const TargetBase &t)
{
    if (cmd)
        return cmd;

    SW_MAKE_COMPILER_COMMAND(driver::cpp::Command);

    getCommandLineOptions<GoCompilerOptions>(c.get(), *this);

    return cmd = c;
}

void GoCompiler::setOutputFile(const path &output_file)
{
    Output = output_file;
    Output() += ".exe";
}

void GoCompiler::setSourceFile(const path &input_file)
{
    InputFiles().insert(input_file);
}

Version GoCompiler::gatherVersion() const
{
    return ::sw::gatherVersion(file, "version");
}

SW_DEFINE_PROGRAM_CLONE(FortranCompiler)

std::shared_ptr<builder::Command> FortranCompiler::prepareCommand(const TargetBase &t)
{
    if (cmd)
        return cmd;

    SW_MAKE_COMPILER_COMMAND(driver::cpp::Command);

    getCommandLineOptions<FortranCompilerOptions>(c.get(), *this);

    return cmd = c;
}

void FortranCompiler::setOutputFile(const path &output_file)
{
    Output = output_file;
    Output() += ".exe";
}

void FortranCompiler::setSourceFile(const path &input_file)
{
    InputFiles().insert(input_file);
}

Version FortranCompiler::gatherVersion() const
{
    return ::sw::gatherVersion(file);
}

SW_DEFINE_PROGRAM_CLONE(JavaCompiler)

std::shared_ptr<builder::Command> JavaCompiler::prepareCommand(const TargetBase &t)
{
    if (cmd)
        return cmd;

    SW_MAKE_COMPILER_COMMAND(driver::cpp::Command);

    getCommandLineOptions<JavaCompilerOptions>(c.get(), *this);

    for (auto &f : InputFiles())
    {
        auto o = OutputDir() / (f.filename().stem() += ".class");
        File(o, *fs).addImplicitDependency(f);
        c->addOutput(o);
    }

    return cmd = c;
}

void JavaCompiler::setOutputDir(const path &output_dir)
{
    OutputDir = output_dir;
}

void JavaCompiler::setSourceFile(const path &input_file)
{
    InputFiles().insert(input_file);
}

Version JavaCompiler::gatherVersion() const
{
    return ::sw::gatherVersion(file, "-version", "(\\d+)\\.(\\d+)\\.(\\d+)(_(\\d+))?");
}

SW_DEFINE_PROGRAM_CLONE(KotlinCompiler)

std::shared_ptr<builder::Command> KotlinCompiler::prepareCommand(const TargetBase &t)
{
    if (cmd)
        return cmd;

    SW_MAKE_COMPILER_COMMAND(driver::cpp::Command);

    getCommandLineOptions<KotlinCompilerOptions>(c.get(), *this);

    return cmd = c;
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

Version KotlinCompiler::gatherVersion() const
{
    return ::sw::gatherVersion(file, "-version");
}

}
