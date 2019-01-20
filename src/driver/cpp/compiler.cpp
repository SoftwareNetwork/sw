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
    SW_MAKE_COMPILER_COMMAND(t)

#define SW_CREATE_COMPILER_COMMAND(t, m, ct)                        \
    std::shared_ptr<driver::cpp::Command> t::createCommand1() const \
    {                                                               \
        m(ct);                                                      \
        return c;                                                   \
    }

static cl::opt<bool> do_not_resolve_compiler("do-not-resolve-compiler");
static cl::opt<bool> use_other_langs("use-other-languages");

namespace sw
{

void detectNativeCompilers(struct Solution &s);
void detectCSharpCompilers(struct Solution &s);
void detectRustCompilers(struct Solution &s);
void detectGoCompilers(struct Solution &s);
void detectFortranCompilers(struct Solution &s);
void detectJavaCompilers(struct Solution &s);
void detectKotlinCompilers(struct Solution &s);
void detectDCompilers(struct Solution &s);

StringSet getCppHeaderFileExtensions()
{
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
    return header_file_extensions;
}

StringSet getCppSourceFileExtensions()
{
    static const StringSet cpp_source_file_extensions{
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
    return cpp_source_file_extensions;
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
    path last_dir = d / s.Settings.TargetOS.Version.toString(true);
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
        detectDCompilers(s);
    }
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
    C->Extension = s.Settings.TargetOS.getExecutableExtension();
    L->compiler = C;
    s.registerProgramAndLanguage("com.Microsoft.VisualStudio.Roslyn.csc", C, L);
#endif
}

void detectWindowsCompilers(struct Solution &s)
{
    // we need ifdef because of cmVSSetupAPIHelper
    // but what if we're on Wine?
    // reconsider later
#ifdef _WIN32

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
        throw SW_RUNTIME_ERROR("Unknown arch");
    }

    switch (s.Settings.TargetOS.Arch)
    {
    case ArchType::x86_64:
        dir_suffix.target = "x64";
        break;
    case ArchType::x86:
        //dir_suffix.host = "x86";
        dir_suffix.target = "x86";
        break;
    case ArchType::arm:
        dir_suffix.target = "arm";
        break;
    case ArchType::aarch64:
        dir_suffix.target = "arm64";
        break;
    default:
        throw SW_RUNTIME_ERROR("Unknown arch");
    }

    auto host_root = compiler / ("Host" + dir_suffix.host) / dir_suffix.host;
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

    // lib, link
    {
        auto Linker = std::make_shared<VisualStudioLinker>();
        Linker->Type = LinkerType::MSVC;
        Linker->file = compiler.parent_path() / "link.exe";
        Linker->vs_version = VSVersion;
        Linker->Extension = s.Settings.TargetOS.getExecutableExtension();
        *Linker = LOpts;
        s.registerProgram("com.Microsoft.VisualStudio.VC.link", Linker);

        if (s.HostOS.Arch != s.Settings.TargetOS.Arch)
        {
            auto c = Linker->createCommand();
            c->addPathDirectory(host_root);
        }

        auto Librarian = std::make_shared<VisualStudioLibrarian>();
        Librarian->Type = LinkerType::MSVC;
        Librarian->file = compiler.parent_path() / "lib.exe";
        Librarian->vs_version = VSVersion;
        Librarian->Extension = s.Settings.TargetOS.getStaticLibraryExtension();
        *Librarian = LOpts;
        s.registerProgram("com.Microsoft.VisualStudio.VC.lib", Librarian);

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
        //L->Type = LanguageType::ASM;
        L->CompiledExtensions = { ".asm" };
        //s.registerLanguage(L);

        //auto L = (ASMLanguage*)s.languages[LanguageType::ASM].get();
        auto C = std::make_shared<VisualStudioASMCompiler>();
        C->Type = CompilerType::MSVC;
        C->file = s.Settings.TargetOS.Arch == ArchType::x86_64 ?
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

        if (s.HostOS.Arch != s.Settings.TargetOS.Arch)
        {
            auto c = C->createCommand();
            c->addPathDirectory(host_root);
        }
    }

    // C++
    {
        auto L = std::make_shared<NativeLanguage>();
        //L->Type = LanguageType::C;
        L->CompiledExtensions = getCppSourceFileExtensions();
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

        if (s.HostOS.Arch != s.Settings.TargetOS.Arch)
        {
            auto c = C->createCommand();
            c->addPathDirectory(host_root);
        }
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
        L->CompiledExtensions = getCppSourceFileExtensions();
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

    // C, C++
    {
        auto L = std::make_shared<NativeLanguage>();
        //L->Type = LanguageType::C;
        L->CompiledExtensions = getCppSourceFileExtensions();
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
#endif
}

void detectNonWindowsCompilers(struct Solution &s)
{
    path p;

    NativeLinkerOptions LOpts;
    //LOpts.System.LinkDirectories.insert("/lib");
    //LOpts.System.LinkDirectories.insert("/lib/x86_64-linux-gnu");
    //LOpts.System.LinkLibraries.push_back("stdc++");
    //LOpts.System.LinkLibraries.push_back("stdc++fs");
    LOpts.System.LinkLibraries.push_back("pthread"); // remove and add to progs explicitly?
    LOpts.System.LinkLibraries.push_back("dl"); // remove and add to progs explicitly?
    LOpts.System.LinkLibraries.push_back("m"); // remove and add to progs explicitly?

    auto resolve = [](const path &p)
    {
        if (do_not_resolve_compiler)
            return p;
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

            if (s.HostOS.is(OSType::Cygwin))
                Linker->rdynamic = false;
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
                L->CompiledExtensions = getCppSourceFileExtensions();
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

                if (s.HostOS.is(OSType::Cygwin))
                    Linker->rdynamic = false;
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
                    L->CompiledExtensions = getCppSourceFileExtensions();
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
}

void detectNativeCompilers(struct Solution &s)
{
    auto &os = s.HostOS;
    if (os.is(OSType::Windows) || os.is(OSType::Cygwin))
    {
        if (os.is(OSType::Cygwin))
            detectNonWindowsCompilers(s);
        detectWindowsCompilers(s);
    }
    else
        detectNonWindowsCompilers(s);
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

SW_CREATE_COMPILER_COMMAND(CompilerBaseProgram, SW_MAKE_COMPILER_COMMAND, driver::cpp::Command)

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

SW_CREATE_COMPILER_COMMAND(VisualStudioCompiler, SW_MAKE_COMPILER_COMMAND_WITH_FILE, driver::cpp::VSCommand)

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
    if (CPPSourceFile)
    {
        cmd->name = normalize_path(CPPSourceFile());
        cmd->name_short = CPPSourceFile().filename().u8string();
        //cmd->file = CPPSourceFile;
    }
    if (ObjectFile)
        cmd->working_directory = ObjectFile().parent_path();

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
    ObjectFile = output_file;
}

SW_DEFINE_PROGRAM_CLONE(VisualStudioCompiler)

void VisualStudioCompiler::setSourceFile(const path &input_file, path &output_file)
{
    InputFile = input_file.u8string();
    VisualStudioCompiler::setOutputFile(output_file);
}

SW_CREATE_COMPILER_COMMAND(VisualStudioASMCompiler, SW_MAKE_COMPILER_COMMAND_WITH_FILE, driver::cpp::VSCommand)

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
    if (ObjectFile)
        cmd->working_directory = ObjectFile().parent_path();

    //if (cmd->file.empty())
        //return nullptr;

    //cmd->out.capture = true;
    //cmd->base = clone();

    getCommandLineOptions<VisualStudioAssemblerOptions>(cmd.get(), *this);
    iterate([this](auto &v, auto &gs) { v.addEverything(*cmd); });
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

SW_CREATE_COMPILER_COMMAND(ClangCompiler, SW_MAKE_COMPILER_COMMAND_WITH_FILE, driver::cpp::GNUCommand)

void ClangCompiler::prepareCommand1(const TargetBase &t)
{
    auto cmd = std::static_pointer_cast<driver::cpp::GNUCommand>(this->cmd);

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
    //cmd->base = clone();

    add_args(*cmd, getClangCppStdOption(CPPStandard()));
    CPPStandard.skip = true;

    getCommandLineOptions<ClangOptions>(cmd.get(), *this);
    iterate([this](auto &v, auto &gs) { v.addEverything(*this->cmd); });
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

SW_CREATE_COMPILER_COMMAND(ClangClCompiler, SW_MAKE_COMPILER_COMMAND_WITH_FILE, driver::cpp::VSCommand)

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
    if (ObjectFile)
        cmd->working_directory = ObjectFile().parent_path();

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
    ObjectFile = output_file;
}

SW_DEFINE_PROGRAM_CLONE(ClangClCompiler)

void ClangClCompiler::setSourceFile(const path &input_file, path &output_file)
{
    InputFile = input_file.u8string();
    setOutputFile(output_file);
}

SW_CREATE_COMPILER_COMMAND(GNUASMCompiler, SW_MAKE_COMPILER_COMMAND_WITH_FILE, driver::cpp::GNUCommand)

void GNUASMCompiler::prepareCommand1(const TargetBase &t)
{
    if (InputFile)
    {
        cmd->name = normalize_path(InputFile());
        cmd->name_short = InputFile().filename().u8string();
        //cmd->file = InputFile;
    }
    if (OutputFile)
        cmd->working_directory = OutputFile().parent_path();

    //if (cmd->file.empty())
        //return nullptr;

    //cmd->out.capture = true;

    getCommandLineOptions<GNUAssemblerOptions>(cmd.get(), *this);
    iterate([this](auto &v, auto &gs) { v.addEverything(*cmd); });
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

SW_CREATE_COMPILER_COMMAND(GNUCompiler, SW_MAKE_COMPILER_COMMAND_WITH_FILE, driver::cpp::GNUCommand)

void GNUCompiler::prepareCommand1(const TargetBase &t)
{
    auto cmd = std::static_pointer_cast<driver::cpp::GNUCommand>(this->cmd);

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

void VisualStudioLibraryTool::prepareCommand1(const TargetBase &t)
{
    if (InputFiles.empty() && DefinitionFile.empty())
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

void VisualStudioLinker::getAdditionalOptions(driver::cpp::Command *cmd) const
{
    getCommandLineOptions<VisualStudioLinkerOptions>(cmd, *this);
}

void VisualStudioLinker::setInputLibraryDependencies(const FilesOrdered &files)
{
    if (!files.empty())
        InputLibraryDependencies().insert(InputLibraryDependencies().end(), files.begin(), files.end());
}

SW_DEFINE_PROGRAM_CLONE(VisualStudioLibrarian)

void VisualStudioLibrarian::getAdditionalOptions(driver::cpp::Command *cmd) const
{
    getCommandLineOptions<VisualStudioLibrarianOptions>(cmd, *this);
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

void GNULinker::getAdditionalOptions(driver::cpp::Command *cmd) const
{
    getCommandLineOptions<GNULinkerOptions>(cmd, *this);
}

void GNULinker::prepareCommand1(const TargetBase &t)
{
    if (InputFiles.empty()/* && DefinitionFile.empty()*/)
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
    ((GNULinker*)this)->GNULinkerOptions::LinkLibraries() = gatherLinkLibraries();

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

void GNULibrarian::getAdditionalOptions(driver::cpp::Command *cmd) const
{
    getCommandLineOptions<GNULibrarianOptions>(cmd, *this);
}

void GNULibrarian::prepareCommand1(const TargetBase &t)
{
    if (InputFiles.empty()/* && DefinitionFile.empty()*/)
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
    OutputFile = output_file;
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
