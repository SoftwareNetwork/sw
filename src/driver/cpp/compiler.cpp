// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <compiler.h>

#include <solution.h>

#include <primitives/sw/settings.h>

#ifdef _WIN32
#include <misc/cmVSSetupHelper.h>
#endif

#include <boost/algorithm/string.hpp>

#include <regex>
#include <string>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "compiler");

static cl::opt<bool> do_not_resolve_compiler("do-not-resolve-compiler");

namespace sw
{

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
    throw std::runtime_error("Unknown VS version");
}

path getProgramFilesX86()
{
    auto e = getenv("programfiles(x86)");
    if (!e)
        throw std::runtime_error("Cannot get 'programfiles(x86)' env. var.");
    return e;
}

bool findDefaultVS2017(path &root, VisualStudioVersion &VSVersion)
{
    auto program_files_x86 = getProgramFilesX86();
    for (auto &edition : { "Enterprise", "Professional", "Community" })
    {
        path p = program_files_x86 / ("Microsoft Visual Studio/2017/"s + edition + "/VC/Auxiliary/Build/vcvarsall.bat");
        if (fs::exists(p))
        {
            root = p.parent_path().parent_path().parent_path();
            VSVersion = VisualStudioVersion::VS15;
            return true;
        }
    }
    return false;
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
    throw std::runtime_error("No Windows Kits available");
}

path getWindowsKit10Dir(Solution &s, const path &d)
{
    // take current or the latest version
    path last_dir = d / s.Settings.HostOS.Version.toString(true);
    if (fs::exists(last_dir))
        return last_dir;
    last_dir.clear();
    for (auto &i : fs::directory_iterator(d))
    {
        if (fs::is_directory(i))
            last_dir = i;
    }
    if (last_dir.empty())
        throw std::runtime_error("No Windows Kits 10.0 available");
    return last_dir;
}

ToolBase::~ToolBase()
{
    //delete cmd;
}

// try to find ALL VS instances on the system
bool VisualStudio::findToolchain(Solution &s) const
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

#if !defined(CPPAN_OS_WINDOWS)
    return false;
#else
    cmVSSetupAPIHelper h;
    if (h.IsVS2017Installed())
    {
        root = h.chosenInstanceInfo.VSInstallLocation;
        root /= "VC";
        VSVersion = VisualStudioVersion::VS15;

        // can be split by points
        static std::wregex r(L"(\\d+)\\.(\\d+)\\.(\\d+)(\\.(\\d+))?");
        std::wsmatch m;
        if (!std::regex_match(h.chosenInstanceInfo.Version, m, r))
            throw std::runtime_error("Cannot match vs version regex");
        if (m[5].matched)
            V = { std::stoi(m[1].str()), std::stoi(m[2].str()), std::stoi(m[3].str()), std::stoi(m[5].str()) };
        else
            V = { std::stoi(m[1].str()), std::stoi(m[2].str()), std::stoi(m[3].str()) };
    }
    else if (!find_comn_tools(VisualStudioVersion::VS15) && !findDefaultVS2017(root, VSVersion))
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
        return false;

    if (VSVersion == VisualStudioVersion::VS15)
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
    switch (s.Settings.HostOS.Arch)
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
    if (VSVersion == VisualStudioVersion::VS15)
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

    auto Linker = std::make_shared<VisualStudioLinker>();
    Linker->Type = LinkerType::MSVC;
    Linker->file = compiler.parent_path() / "link.exe";
    Linker->vs_version = VSVersion;
    if (s.Settings.TargetOS.Arch == ArchType::x86)
        Linker->Machine = vs::MachineType::X86;
    *Linker = LOpts;

    auto Librarian = std::make_shared<VisualStudioLibrarian>();
    Librarian->Type = LinkerType::MSVC;
    Librarian->file = compiler.parent_path() / "lib.exe";
    Librarian->vs_version = VSVersion;
    if (s.Settings.TargetOS.Arch == ArchType::x86)
        Librarian->Machine = vs::MachineType::X86;
    *Librarian = LOpts;

    // ASM
    {
        auto L = (ASMLanguage*)s.languages[LanguageType::ASM].get();
        auto C = std::make_shared<VisualStudioASMCompiler>();
        C->Type = CompilerType::MSVC;
        C->file = s.Settings.HostOS.Arch == ArchType::x86_64 ?
            (compiler.parent_path() / "ml64.exe") :
            (compiler.parent_path() / "ml.exe");
        C->vs_version = VSVersion;
        *C = COpts;
        L->compiler = C;
        L->librarian = Librarian;
        L->linker = Linker;
    }

    // C
    {
        auto L = (CLanguage*)s.languages[LanguageType::C].get();
        auto C = std::make_shared<VisualStudioCCompiler>();
        C->Type = CompilerType::MSVC;
        C->file = compiler;
        C->vs_version = VSVersion;
        *C = COpts;
        L->compiler = C;
        L->librarian = Librarian;
        L->linker = Linker;
    }

    // CPP
    {
        auto L = (CPPLanguage*)s.languages[LanguageType::CPP].get();
        auto C = std::make_shared<VisualStudioCPPCompiler>();
        C->Type = CompilerType::MSVC;
        C->file = compiler;
        C->vs_version = VSVersion;
        *C = COpts;
        L->compiler = C;
        L->librarian = Librarian;
        L->linker = Linker;
    }

    return true;
#endif
}

bool Clang::findToolchain(struct Solution &s) const
{
    // implemented for windows only now

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
            root /= "../../VC/";
            VSVersion = v;
            return true;
        }
        return false;
    };

#if !defined(CPPAN_OS_WINDOWS)
    return false;
#else
    cmVSSetupAPIHelper h;
    if (h.IsVS2017Installed())
    {
        root = h.chosenInstanceInfo.VSInstallLocation;
        root /= "VC";
        VSVersion = VisualStudioVersion::VS15;

        // can be split by points
        static std::wregex r(L"(\\d+)\\.(\\d+)\\.(\\d+)(\\.(\\d+))?");
        std::wsmatch m;
        if (!std::regex_match(h.chosenInstanceInfo.Version, m, r))
            throw std::runtime_error("Cannot match vs version regex");
        if (m[5].matched)
            V = { std::stoi(m[1].str()), std::stoi(m[2].str()), std::stoi(m[3].str()), std::stoi(m[5].str()) };
        else
            V = { std::stoi(m[1].str()), std::stoi(m[2].str()), std::stoi(m[3].str()) };
    }
    else if (!find_comn_tools(VisualStudioVersion::VS15) && !findDefaultVS2017(root, VSVersion))
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
        return false;

    if (VSVersion == VisualStudioVersion::VS15)
        root = root / "Tools/MSVC" / boost::trim_copy(read_file(root / "Auxiliary/Build/Microsoft.VCToolsVersion.default.txt"));

    auto ToolSet = getVsToolset(VSVersion);
    auto compiler = root / "bin";
    NativeCompilerOptions COpts;
    COpts.System.IncludeDirectories.insert(root / "include");
    COpts.System.IncludeDirectories.insert(root / "ATLMFC/include"); // also add

    struct DirSuffix
    {
        std::string include;
        std::string lib;
    } dir_suffix;

    // get suffix
    switch (s.Settings.HostOS.Arch)
    {
    case ArchType::x86_64:
        dir_suffix.include = "x64";
        dir_suffix.lib = "x64";
        break;
    case ArchType::x86:
        dir_suffix.include = "x86";
        dir_suffix.lib = "x86";
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
    if (VSVersion == VisualStudioVersion::VS15)
    {
        // always use host tools and host arch for building config files
        compiler /= "Host" + dir_suffix.lib + "/" + dir_suffix.lib + "/cl.exe";
        LOpts.System.LinkDirectories.insert(root / ("lib/" + dir_suffix.lib));
        LOpts.System.LinkDirectories.insert(root / ("ATLMFC/lib/" + dir_suffix.lib)); // also add
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
            LOpts.System.LinkDirectories.insert(i / path(dir_suffix.lib));
    }

    // create programs

    const path base_llvm_path = "c:\\Program Files\\LLVM\\bin";

    auto Linker = std::make_shared<VisualStudioLinker>();
    Linker->Type = LinkerType::LLD;
    Linker->file = base_llvm_path / "lld-link.exe";
    Linker->vs_version = VSVersion;
    *Linker = LOpts;

    auto Librarian = std::make_shared<VisualStudioLibrarian>();
    Librarian->Type = LinkerType::LLD;
    Librarian->file = base_llvm_path / "llvm-ar.exe"; // ?
    Librarian->vs_version = VSVersion;
    *Librarian = LOpts;

    // C
    {
        auto L = (CLanguage*)s.languages[LanguageType::C].get();
        auto C = std::make_shared<ClangCCompiler>();
        C->Type = CompilerType::Clang;
        C->file = base_llvm_path / "clang.exe";
        *C = COpts;
        L->compiler = C;
        L->librarian = Librarian;
        L->linker = Linker;
    }

    // CPP
    {
        auto L = (CPPLanguage*)s.languages[LanguageType::CPP].get();
        auto C = std::make_shared<ClangCPPCompiler>();
        C->Type = CompilerType::Clang;
        C->file = base_llvm_path / "clang++.exe";
        *C = COpts;
        L->compiler = C;
        L->librarian = Librarian;
        L->linker = Linker;
    }

    return true;
#endif
}

bool ClangCl::findToolchain(struct Solution &s) const
{
    // implemented for windows only now

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
            root /= "../../VC/";
            VSVersion = v;
            return true;
        }
        return false;
    };

#if !defined(CPPAN_OS_WINDOWS)
    return false;
#else
    cmVSSetupAPIHelper h;
    if (h.IsVS2017Installed())
    {
        root = h.chosenInstanceInfo.VSInstallLocation;
        root /= "VC";
        VSVersion = VisualStudioVersion::VS15;

        // can be split by points
        static std::wregex r(L"(\\d+)\\.(\\d+)\\.(\\d+)(\\.(\\d+))?");
        std::wsmatch m;
        if (!std::regex_match(h.chosenInstanceInfo.Version, m, r))
            throw std::runtime_error("Cannot match vs version regex");
        if (m[5].matched)
            V = { std::stoi(m[1].str()), std::stoi(m[2].str()), std::stoi(m[3].str()), std::stoi(m[5].str()) };
        else
            V = { std::stoi(m[1].str()), std::stoi(m[2].str()), std::stoi(m[3].str()) };
    }
    else if (!find_comn_tools(VisualStudioVersion::VS15) && !findDefaultVS2017(root, VSVersion))
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
        return false;

    if (VSVersion == VisualStudioVersion::VS15)
        root = root / "Tools/MSVC" / boost::trim_copy(read_file(root / "Auxiliary/Build/Microsoft.VCToolsVersion.default.txt"));

    auto ToolSet = getVsToolset(VSVersion);
    auto compiler = root / "bin";
    NativeCompilerOptions COpts;
    COpts.System.IncludeDirectories.insert(root / "include");
    COpts.System.IncludeDirectories.insert(root / "ATLMFC/include"); // also add

    struct DirSuffix
    {
        std::string include;
        std::string lib;
    } dir_suffix;

    // get suffix
    switch (s.Settings.HostOS.Arch)
    {
    case ArchType::x86_64:
        dir_suffix.include = "x64";
        dir_suffix.lib = "x64";
        break;
    case ArchType::x86:
        dir_suffix.include = "x86";
        dir_suffix.lib = "x86";
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
    if (VSVersion == VisualStudioVersion::VS15)
    {
        // always use host tools and host arch for building config files
        compiler /= "Host" + dir_suffix.lib + "/" + dir_suffix.lib + "/cl.exe";
        LOpts.System.LinkDirectories.insert(root / ("lib/" + dir_suffix.lib));
        LOpts.System.LinkDirectories.insert(root / ("ATLMFC/lib/" + dir_suffix.lib)); // also add
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
            LOpts.System.LinkDirectories.insert(i / path(dir_suffix.lib));
    }

    // create programs

    auto Linker = std::make_shared<VisualStudioLinker>();
    Linker->Type = LinkerType::MSVC;
    Linker->file = compiler.parent_path() / "link.exe";
    Linker->vs_version = VSVersion;
    *Linker = LOpts;

    auto Librarian = std::make_shared<VisualStudioLibrarian>();
    Librarian->Type = LinkerType::MSVC;
    Librarian->file = compiler.parent_path() / "lib.exe";
    Librarian->vs_version = VSVersion;
    *Librarian = LOpts;

    const path base_llvm_path = "c:\\Program Files\\LLVM\\bin";

    // C
    {
        auto L = (CLanguage*)s.languages[LanguageType::C].get();
        auto C = std::make_shared<ClangClCCompiler>();
        C->Type = CompilerType::Clang;
        C->file = base_llvm_path / "clang-cl.exe";
        *C = COpts;
        L->compiler = C;
        L->librarian = Librarian;
        L->linker = Linker;
    }

    // CPP
    {
        auto L = (CPPLanguage*)s.languages[LanguageType::CPP].get();
        auto C = std::make_shared<ClangClCPPCompiler>();
        C->Type = CompilerType::Clang;
        C->file = base_llvm_path / "clang-cl.exe";
        *C = COpts;
        L->compiler = C;
        L->librarian = Librarian;
        L->linker = Linker;
    }

    return true;
#endif
}

bool GNU::findToolchain(struct Solution &s) const
{
    path p;

    NativeLinkerOptions LOpts;
    LOpts.System.LinkDirectories.insert("/lib");
    LOpts.System.LinkDirectories.insert("/lib/x86_64-linux-gnu");
    LOpts.System.LinkLibraries.insert("stdc++");
    LOpts.System.LinkLibraries.insert("stdc++fs");
    LOpts.System.LinkLibraries.insert("pthread");
    LOpts.System.LinkLibraries.insert("dl");

    auto resolve = [](const path &p)
    {
        if (do_not_resolve_compiler)
            return p;
        return primitives::resolve_executable(p);
    };

    p = resolve("ar");
    if (p.empty())
        throw std::runtime_error("cannot find ar");

    auto Librarian = std::make_shared<GNULibrarian>();
    Librarian->Type = LinkerType::GNU;
    Librarian->file = p;
    *Librarian = LOpts;

    //p = resolve("ld.gold");
    p = resolve("gcc-8");
    if (p.empty())
        throw std::runtime_error("cannot find gcc");

    auto Linker = std::make_shared<GNULinker>();
    Linker->Type = LinkerType::GNU;
    Linker->file = p;
    *Linker = LOpts;

    NativeCompilerOptions COpts;
    //COpts.System.IncludeDirectories.insert("/usr/include");
    //COpts.System.IncludeDirectories.insert("/usr/include/x86_64-linux-gnu");

    p = resolve("gcc-8");
    if (!p.empty())
    {
        // C
        {
            auto L = (CLanguage*)s.languages[LanguageType::C].get();
            auto C = std::make_shared<GNUCCompiler>();
            C->Type = CompilerType::GNU;
            C->file = p;
            *C = COpts;
            L->compiler = C;
            L->librarian = Librarian;
            L->linker = Linker;
        }
    }

    p = resolve("g++-8");
    if (!p.empty())
    {
        // CPP
        {
            auto L = (CPPLanguage*)s.languages[LanguageType::CPP].get();
            auto C = std::make_shared<GNUCPPCompiler>();
            C->Type = CompilerType::GNU;
            C->file = p;
            *C = COpts;
            L->compiler = C;
            L->librarian = Librarian;
            L->linker = Linker;
        }
    }

    // check gcc-N, N=4..8
    return true;
}

template <class T>
static void getCommandLineOptions(driver::cpp::Command *c, const CommandLineOptions<T> &t, const String prefix = "", bool end_options = false)
{
    for (auto &o : t)
    {
        if (o.manual_handling)
            continue;
        if (end_options != o.place_at_the_end)
            continue;
        auto cmd = o.getCommandLine(c);
        for (auto &c2 : cmd)
        {
            if (!prefix.empty())
                c->args.push_back(prefix);
            c->args.push_back(c2);
        }
    }
}

template <class V, class GS>
static void printDefsAndIdirs(driver::cpp::Command *c, V &v, const GS &gs)
{
    auto print_def = [&c](auto &a)
    {
        for (auto &d : a)
        {
            using namespace sw;

            if (d.second.empty()/* || d.first.find("=") != -1*/)
                c->args.push_back("-D" + d.first);
            else
                c->args.push_back("-D" + d.first + "=" + d.second);
        }
    };

    print_def(v.System.Definitions);
    print_def(v.Definitions);

    auto print_idir = [&c](const auto &a, auto &flag)
    {
        for (auto &d : a)
            c->args.push_back(flag + normalize_path(d));
    };

    print_idir(v.gatherIncludeDirectories(), "-I");
    print_idir(v.System.gatherIncludeDirectories(),
           #ifdef _WIN32
               "-I"
           #else
               //"-isystem"
               "-I"
           #endif
               );
}

Version VisualStudio::gatherVersion(const path &program) const
{
    Version V;
    primitives::Command c;
    c.program = program;
    c.args = { "--version" };
    std::error_code ec;
    c.execute(ec);
    // ms returns exit code = 2 on --version
    if (ec)
    {
        static std::regex r("(\\d+)\\.(\\d+)\\.(\\d+)(\\.(\\d+))?");
        std::smatch m;
        if (std::regex_search(c.err.text, m, r))
        {
            if (m[5].matched)
                V = { std::stoi(m[1].str()), std::stoi(m[2].str()), std::stoi(m[3].str()), std::stoi(m[5].str()) };
            else
                V = { std::stoi(m[1].str()), std::stoi(m[2].str()), std::stoi(m[3].str()) };
        }
    }
    return V;
}

std::shared_ptr<builder::Command> VisualStudioASMCompiler::getCommand() const
{
    struct VSAsmCommand : driver::cpp::Command
    {
        File file;

        virtual void postProcess(bool)
        {
            // filter out includes and file name
            static const auto pattern = "Note: including file:"s;

            std::deque<String> lines;
            boost::split(lines, out.text, boost::is_any_of("\n"));
            out.text.clear();
            // remove filename
            lines.pop_front();

            file.clearImplicitDependencies();

            for (auto &line : lines)
            {
                auto p = line.find(pattern);
                if (p != 0)
                {
                    out.text += line + "\n";
                    continue;
                }
                auto include = line.substr(pattern.size());
                boost::trim(include);
                file.addImplicitDependency(include);
            }
        }
    };

    if (cmd)
        return cmd;

    auto c = std::make_shared<VSAsmCommand>();

    if (file.filename() == "ml64.exe")
        ((VisualStudioASMCompiler*)this)->SafeSEH = false;

    if (InputFile)
    {
        c->name = normalize_path(InputFile());
        c->name_short = InputFile().filename().string();
        c->file = InputFile;
    }
    if (ObjectFile)
        c->working_directory = ObjectFile().parent_path();

    if (c->file.empty())
        return nullptr;

    //c->out.capture = true;
    c->base = clone();

    getCommandLineOptions<VisualStudioAssemblerOptions>(c.get(), *this);
    iterate([c](auto &v, auto &gs) { printDefsAndIdirs(c.get(), v, gs); });

    return cmd = c;
}

std::shared_ptr<Program> VisualStudioASMCompiler::clone() const
{
    return std::make_shared<VisualStudioASMCompiler>(*this);
}

void VisualStudioASMCompiler::setOutputFile(const path &output_file)
{
    ObjectFile = output_file;
}

Files VisualStudioASMCompiler::getGeneratedDirs() const
{
    Files f;
    f.insert(ObjectFile().parent_path());
    return f;
}

void VisualStudioASMCompiler::setSourceFile(const path &input_file, path &output_file)
{
    InputFile = input_file.string();
    setOutputFile(output_file);
}

std::shared_ptr<builder::Command> VisualStudioCompiler::getCommand() const
{
    struct VSCompilerCommand : driver::cpp::Command
    {
        File file;

        virtual void postProcess(bool)
        {
            // filter out includes and file name
            static const auto pattern = "Note: including file:"s;

            std::deque<String> lines;
            boost::split(lines, out.text, boost::is_any_of("\n"));
            out.text.clear();
            // remove filename
            lines.pop_front();

            file.clearImplicitDependencies();

            for (auto &line : lines)
            {
                auto p = line.find(pattern);
                if (p != 0)
                {
                    out.text += line + "\n";
                    continue;
                }
                auto include = line.substr(pattern.size());
                boost::trim(include);
                file.addImplicitDependency(include);
            }
        }
    };

    if (cmd)
        return cmd;

    auto c = std::make_shared<VSCompilerCommand>();

    if (InputFile)
    {
        c->name = normalize_path(InputFile());
        c->name_short = InputFile().filename().string();
        c->file = InputFile;
    }
    if (CSourceFile)
    {
        c->name = normalize_path(CSourceFile());
        c->name_short = CSourceFile().filename().string();
        c->file = CSourceFile;
    }
    if (CPPSourceFile)
    {
        c->name = normalize_path(CPPSourceFile());
        c->name_short = CPPSourceFile().filename().string();
        c->file = CPPSourceFile;
    }
    if (ObjectFile)
        c->working_directory = ObjectFile().parent_path();

    if (c->file.empty())
        return nullptr;

    //c->out.capture = true;
    c->base = clone();

    getCommandLineOptions<VisualStudioCompilerOptions>(c.get(), *this);
    iterate([c](auto &v, auto &gs) { printDefsAndIdirs(c.get(), v, gs); });

    if (PreprocessToFile)
    {
        //c->addOutput(c->file.file.parent_path() / (c->file.file.filename().stem().string() + ".i"));
        // TODO: remove old object file, it's now incorrect
    }

    return cmd = c;
}

void VisualStudioCompiler::setOutputFile(const path &output_file)
{
    ObjectFile = output_file;
}

Files VisualStudioCompiler::getGeneratedDirs() const
{
    Files f;
    f.insert(ObjectFile().parent_path());
    return f;
}

std::shared_ptr<Program> VisualStudioCCompiler::clone() const
{
    return std::make_shared<VisualStudioCCompiler>(*this);
}

void VisualStudioCCompiler::setSourceFile(const path &input_file, path &output_file)
{
    CSourceFile = input_file.string();
    VisualStudioCompiler::setOutputFile(output_file);
}

std::shared_ptr<Program> VisualStudioCPPCompiler::clone() const
{
    return std::make_shared<VisualStudioCPPCompiler>(*this);
}

void VisualStudioCPPCompiler::setSourceFile(const path &input_file, path &output_file)
{
    CPPSourceFile = input_file.string();
    VisualStudioCompiler::setOutputFile(output_file);
}

Version Clang::gatherVersion(const path &program) const
{
    Version v;
    primitives::Command c;
    c.program = program;
    c.args = {"-v"};
    std::error_code ec;
    c.execute(ec);
    if (!ec)
    {
        static std::regex r("^clang version (\\d+).(\\d+).(\\d+)");
        std::smatch m;
        if (std::regex_search(c.err.text, m, r))
            v = { std::stoi(m[1].str()), std::stoi(m[2].str()), std::stoi(m[3].str()) };
    }
    return v;
}

std::shared_ptr<builder::Command> ClangCompiler::getCommand() const
{
    struct ClangCompilerCommand : driver::cpp::Command
    {
        File file;
        path deps_file;

        virtual void postProcess(bool ok)
        {
            if (!ok || deps_file.empty())
                return;

            static const std::regex space_r("[^\\\\] ");

            auto lines = read_lines(deps_file);
            file.clearImplicitDependencies();
            for (auto i = lines.begin() + 1; i != lines.end(); i++)
            {
                auto &s = *i;
                s.resize(s.size() - 1);
                boost::trim(s);
                s = std::regex_replace(s, space_r, "\n");
                boost::replace_all(s, "\\ ", " ");
                Strings files;
                boost::split(files, s, boost::is_any_of("\n"));
                //boost::replace_all(s, "\\\"", "\""); // probably no quotes
                for (auto &f : files)
                    file.addImplicitDependency(f);
            }
        }
    };

    if (cmd)
        return cmd;

    auto c = std::make_shared<ClangCompilerCommand>();
    if (InputFile)
    {
        c->name = normalize_path(InputFile());
        c->name_short = InputFile().filename().string();
        c->file = InputFile;
    }
    if (OutputFile)
        c->deps_file = OutputFile().parent_path() / (OutputFile().stem().string() + ".d");
    c->working_directory = OutputFile().parent_path();

    if (c->file.empty())
        return nullptr;

    //c->out.capture = true;
    c->base = clone();

    getCommandLineOptions<ClangOptions>(c.get(), *this);
    iterate([c](auto &v, auto &gs) { printDefsAndIdirs(c.get(), v, gs); });

    return cmd = c;
}

void ClangCompiler::setOutputFile(const path &output_file)
{
    OutputFile = output_file;
}

Files ClangCompiler::getGeneratedDirs() const
{
    Files f;
    f.insert(OutputFile().parent_path());
    return f;
}

std::shared_ptr<Program> ClangCCompiler::clone() const
{
    return std::make_shared<ClangCCompiler>(*this);
}

void ClangCCompiler::setSourceFile(const path &input_file, path &output_file)
{
    InputFile = input_file;
    ClangCompiler::setOutputFile(output_file);
}

std::shared_ptr<Program> ClangCPPCompiler::clone() const
{
    return std::make_shared<ClangCPPCompiler>(*this);
}

void ClangCPPCompiler::setSourceFile(const path &input_file, path &output_file)
{
    InputFile = input_file;
    ClangCompiler::setOutputFile(output_file);
}

std::shared_ptr<builder::Command> ClangClCompiler::getCommand() const
{
    struct VSCompilerCommand : driver::cpp::Command
    {
        File file;

        virtual void postProcess(bool)
        {
            // filter out includes and file name
            static const auto pattern = "Note: including file:"s;

            std::deque<String> lines;
            boost::split(lines, out.text, boost::is_any_of("\n"));
            out.text.clear();
            // remove filename
            lines.pop_front();

            file.clearImplicitDependencies();

            for (auto &line : lines)
            {
                auto p = line.find(pattern);
                if (p != 0)
                {
                    out.text += line + "\n";
                    continue;
                }
                auto include = line.substr(pattern.size());
                boost::trim(include);
                file.addImplicitDependency(include);
            }
        }
    };

    if (cmd)
        return cmd;

    auto c = std::make_shared<VSCompilerCommand>();
    if (InputFile)
    {
        c->name = normalize_path(InputFile());
        c->name_short = InputFile().filename().string();
        c->file = InputFile;
    }
    if (CSourceFile)
    {
        c->name = normalize_path(CSourceFile());
        c->name_short = CSourceFile().filename().string();
        c->file = CSourceFile;
    }
    if (CPPSourceFile)
    {
        c->name = normalize_path(CPPSourceFile());
        c->name_short = CPPSourceFile().filename().string();
        c->file = CPPSourceFile;
    }
    if (ObjectFile)
        c->working_directory = ObjectFile().parent_path();

    if (c->file.empty())
        return nullptr;

    //c->out.capture = true;
    c->base = clone();

    getCommandLineOptions<VisualStudioCompilerOptions>(c.get(), *this);
    getCommandLineOptions<ClangClOptions>(c.get(), *this, "-Xclang");
    iterate([c](auto &v, auto &gs) { printDefsAndIdirs(c.get(), v, gs); });

    return cmd = c;
}

void ClangClCompiler::setOutputFile(const path &output_file)
{
    ObjectFile = output_file;
}

Files ClangClCompiler::getGeneratedDirs() const
{
    Files f;
    f.insert(ObjectFile().parent_path());
    return f;
}

std::shared_ptr<Program> ClangClCCompiler::clone() const
{
    return std::make_shared<ClangClCCompiler>(*this);
}

std::shared_ptr<Program> ClangClCPPCompiler::clone() const
{
    return std::make_shared<ClangClCPPCompiler>(*this);
}

void ClangClCCompiler::setSourceFile(const path &input_file, path &output_file)
{
    CSourceFile = input_file.string();
    setOutputFile(output_file);
}

void ClangClCPPCompiler::setSourceFile(const path &input_file, path &output_file)
{
    CPPSourceFile = input_file.string();
    setOutputFile(output_file);
}

Version GNU::gatherVersion(const path &program) const
{
    Version v;
    primitives::Command c;
    c.program = program;
    c.args = { "-v" };
    std::error_code ec;
    c.execute(ec);
    if (!ec)
    {
        static std::regex r("(\\d+).(\\d+).(\\d+)");
        std::smatch m;
        if (std::regex_search(c.err.text, m, r))
            v = { std::stoi(m[1].str()), std::stoi(m[2].str()), std::stoi(m[3].str()) };
    }
    return v;
}

std::shared_ptr<builder::Command> GNUCompiler::getCommand() const
{
    struct GNUCompilerCommand : driver::cpp::Command
    {
        File file;
        path deps_file;

        virtual void postProcess(bool ok)
        {
            if (!ok || deps_file.empty())
                return;

            static const std::regex space_r("[^\\\\] ");

            error_code ec;
            if (!fs::exists(deps_file))
                return;

            auto lines = read_lines(deps_file);
            file.clearImplicitDependencies();
            for (auto i = lines.begin() + 1; i != lines.end(); i++)
            {
                auto &s = *i;
                s.resize(s.size() - 1);
                boost::trim(s);
                s = std::regex_replace(s, space_r, "\n");
                boost::replace_all(s, "\\ ", " ");
                Strings files;
                boost::split(files, s, boost::is_any_of("\n"));
                //boost::replace_all(s, "\\\"", "\""); // probably no quotes
                for (auto &f : files)
                    file.addImplicitDependency(f);
            }
        }
    };

    if (cmd)
        return cmd;

    auto c = std::make_shared<GNUCompilerCommand>();
    if (InputFile)
    {
        c->name = normalize_path(InputFile());
        c->name_short = InputFile().filename().string();
        c->file = InputFile;
    }
    if (OutputFile)
        c->deps_file = OutputFile().parent_path() / (OutputFile().stem().string() + ".d");
    c->working_directory = OutputFile().parent_path();

    if (c->file.empty())
        return nullptr;

    //c->out.capture = true;
    c->base = clone();

    getCommandLineOptions<GNUOptions>(c.get(), *this);
    iterate([c](auto &v, auto &gs) { printDefsAndIdirs(c.get(), v, gs); });
    getCommandLineOptions<GNUOptions>(c.get(), *this, "", true);

    return cmd = c;
}

void GNUCompiler::setOutputFile(const path &output_file)
{
    OutputFile = output_file;
}

Files GNUCompiler::getGeneratedDirs() const
{
    Files f;
    f.insert(OutputFile().parent_path());
    return f;
}

std::shared_ptr<Program> GNUCCompiler::clone() const
{
    return std::make_shared<GNUCCompiler>(*this);
}

void GNUCCompiler::setSourceFile(const path &input_file, path &output_file)
{
    InputFile = input_file;
    GNUCompiler::setOutputFile(output_file);
}

std::shared_ptr<Program> GNUCPPCompiler::clone() const
{
    return std::make_shared<GNUCPPCompiler>(*this);
}

void GNUCPPCompiler::setSourceFile(const path &input_file, path &output_file)
{
    InputFile = input_file;
    GNUCompiler::setOutputFile(output_file);
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
    Output = out.string() + Extension;
}

void VisualStudioLibraryTool::setImportLibrary(const path &out)
{
    ImportLibrary = out.string() + ".lib";
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

std::shared_ptr<builder::Command> VisualStudioLibraryTool::getCommand() const
{
    if (cmd)
        return cmd;

    if (InputFiles.empty() && DefinitionFile.empty())
        return nullptr;

    if (Output.empty())
        throw std::runtime_error("Output file is not set");

    // can be zero imput files actually: lib.exe /DEF:my.def /OUT:x.lib
    //if (InputFiles().empty())
        //return nullptr;

    //LinkDirectories() = gatherLinkDirectories();
    //LinkLibraries() = gatherLinkLibraries();

    auto c = std::make_shared<driver::cpp::Command>();
    //c->out.capture = true;
    c->base = clone();
    c->working_directory = Output().parent_path();

    c->name = normalize_path(Output());
    c->name_short = Output().filename().string();

    /*if (c->name.find("eccdata.exe") != -1)
    {
        int a = 5;
        a++;
    }*/

    ((VisualStudioLibraryTool*)this)->VisualStudioLibraryToolOptions::LinkDirectories() = gatherLinkDirectories();

    getCommandLineOptions<VisualStudioLibraryToolOptions>(c.get(), *this);
    getAdditionalOptions(c.get());

    return cmd = c;
}

VisualStudioLinker::VisualStudioLinker()
{
    Extension = ".exe";
}

std::shared_ptr<Program> VisualStudioLinker::clone() const
{
    return std::make_shared<VisualStudioLinker>(*this);
}

void VisualStudioLinker::getAdditionalOptions(driver::cpp::Command *c) const
{
    getCommandLineOptions<VisualStudioLinkerOptions>(c, *this);
}

VisualStudioLibrarian::VisualStudioLibrarian()
{
    Extension = ".lib";
}

std::shared_ptr<Program> VisualStudioLibrarian::clone() const
{
    return std::make_shared<VisualStudioLibrarian>(*this);
}

void VisualStudioLibrarian::getAdditionalOptions(driver::cpp::Command *c) const
{
    getCommandLineOptions<VisualStudioLibrarianOptions>(c, *this);
}

GNULinker::GNULinker()
{
    //Extension = ".exe";
}

std::shared_ptr<Program> GNULinker::clone() const
{
    return std::make_shared<GNULinker>(*this);
}

void GNULinker::setObjectFiles(const Files &files)
{
    if (!files.empty())
        InputFiles().insert(files.begin(), files.end());
}

void GNULinker::setOutputFile(const path &out)
{
    Output = out.string() + Extension;
}

void GNULinker::setImportLibrary(const path &out)
{
    //ImportLibrary = out.string();// + ".lib";
}

void GNULinker::setLinkLibraries(const FilesOrdered &in)
{
    for (auto &lib : in)
        NativeLinker::LinkLibraries.insert(lib);
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

std::shared_ptr<builder::Command> GNULinker::getCommand() const
{
    if (cmd)
        return cmd;

    if (InputFiles.empty()/* && DefinitionFile.empty()*/)
        return nullptr;

    if (Output.empty())
        throw std::runtime_error("Output file is not set");

    // can be zero imput files actually: lib.exe /DEF:my.def /OUT:x.lib
    //if (InputFiles().empty())
        //return nullptr;

    //LinkDirectories() = gatherLinkDirectories();
    ((GNULinker*)this)->GNULinkerOptions::LinkLibraries() = gatherLinkLibraries();

    auto c = std::make_shared<driver::cpp::Command>();
    //c->out.capture = true;
    c->base = clone();
    c->working_directory = Output().parent_path();

    c->name = normalize_path(Output());
    c->name_short = Output().filename().string();

    /*if (c->name.find("eccdata.exe") != -1)
    {
        int a = 5;
        a++;
    }*/

    //((GNULibraryTool*)this)->GNULibraryToolOptions::LinkDirectories() = gatherLinkDirectories();

    getCommandLineOptions<GNULinkerOptions>(c.get(), *this);
    //getAdditionalOptions(c.get());

    return cmd = c;
}

GNULibrarian::GNULibrarian()
{
    Extension = ".a";
}

std::shared_ptr<Program> GNULibrarian::clone() const
{
    return std::make_shared<GNULibrarian>(*this);
}

void GNULibrarian::setObjectFiles(const Files &files)
{
    if (!files.empty())
        InputFiles().insert(files.begin(), files.end());
}

void GNULibrarian::setOutputFile(const path &out)
{
    Output = out.string() + Extension;
}

void GNULibrarian::setImportLibrary(const path &out)
{
    //ImportLibrary = out.string();// + ".lib";
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

std::shared_ptr<builder::Command> GNULibrarian::getCommand() const
{
    if (cmd)
        return cmd;

    if (InputFiles.empty()/* && DefinitionFile.empty()*/)
        return nullptr;

    if (Output.empty())
        throw std::runtime_error("Output file is not set");

    // can be zero imput files actually: lib.exe /DEF:my.def /OUT:x.lib
    //if (InputFiles().empty())
        //return nullptr;

    //LinkDirectories() = gatherLinkDirectories();
    //LinkLibraries() = gatherLinkLibraries();

    auto c = std::make_shared<driver::cpp::Command>();
    //c->out.capture = true;
    c->base = clone();
    c->working_directory = Output().parent_path();

    c->name = normalize_path(Output());
    c->name_short = Output().filename().string();

    /*if (c->name.find("eccdata.exe") != -1)
    {
        int a = 5;
        a++;
    }*/

    //((GNULibraryTool*)this)->GNULibraryToolOptions::LinkDirectories() = gatherLinkDirectories();

    getCommandLineOptions<GNULibrarianOptions>(c.get(), *this);
    //getAdditionalOptions(c.get());

    return cmd = c;
}

String NativeToolchain::getConfig() const
{
    String c;

    addConfigElement(c, toString(CompilerType));
    addConfigElement(c, CPPCompiler->getVersion().toString(2));
    addConfigElement(c, toString(LibrariesType));
    boost::to_lower(c);
    addConfigElement(c, toString(ConfigurationType));

    return c;
}

}
