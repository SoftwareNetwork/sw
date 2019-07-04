// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "sw/core/sw_context.h"

#include "misc/cmVSSetupHelper.h"

#include <sw/builder/program.h>
#include <sw/builder/program_version_storage.h>

#include <boost/algorithm/string.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/lock_types.hpp>

#include <regex>
#include <string>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "compiler.detect");

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

std::string getVsToolset(const Version &v);

void detectNativeCompilers(SwCoreContext &s);
void detectCSharpCompilers(SwCoreContext &s);
void detectRustCompilers(SwCoreContext &s);
void detectGoCompilers(SwCoreContext &s);
void detectFortranCompilers(SwCoreContext &s);
void detectJavaCompilers(SwCoreContext &s);
void detectKotlinCompilers(SwCoreContext &s);
void detectDCompilers(SwCoreContext &s);

static Version gatherVersion(const path &program, const String &arg = "--version", const String &in_regex = {})
{
    static std::regex r_default("(\\d+)\\.(\\d+)\\.(\\d+)(\\.(\\d+))?");

    std::regex r_in;
    if (!in_regex.empty())
        r_in.assign(in_regex);

    auto &r = in_regex.empty() ? r_default : r_in;

    Version V;
    builder::detail::ResolvableCommand c; // for nice program resolving
    c.setProgram(program);
    if (!arg.empty())
        c.arguments = { arg };
    error_code ec;
    c.execute(ec);

    if (c.pid == -1)
        throw SW_RUNTIME_ERROR(normalize_path(program) + ": " + ec.message());

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

static Version getVersion(SwCoreContext &swctx, const path &program, const String &arg = "--version", const String &in_regex = {})
{
    auto &vs = swctx.getVersionStorage();
    static boost::upgrade_mutex m;

    boost::upgrade_lock lk(m);
    auto i = vs.versions.find(program);
    if (i != vs.versions.end())
        return i->second;

    boost::upgrade_to_unique_lock lk2(lk);

    vs.versions[program] = gatherVersion(program, arg, in_regex);
    return vs.versions[program];
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

void detectCompilers(SwCoreContext &s)
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

// left join comparator

struct PredefinedTarget : ITarget
{
    PackageId id;
    TargetSettings ts;
    TargetSettings public_ts;

    PredefinedTarget(const PackageId &id) :id(id) {}
    virtual ~PredefinedTarget() {}

    const PackageId &getPackage() const override { return id; }

    /// can be registered to software network
    bool isReal() const override { return false; }

    const Source &getSource() const override { SW_UNIMPLEMENTED; }
    Files getSourceFiles() const override { SW_UNIMPLEMENTED; }
    std::vector<IDependency *> getDependencies() const override { SW_UNIMPLEMENTED; }
    bool prepare() override { SW_UNIMPLEMENTED; }
    Commands getCommands() const override { SW_UNIMPLEMENTED; }

    bool operator==(const TargetSettings &s) const override { return ts == s; }
    //bool operator<(const TargetSettings &s) const override { return ts < s; }
};

struct PredefinedProgramTarget : PredefinedTarget, PredefinedProgram
{
    using PredefinedTarget::PredefinedTarget;
};

template <class T>
static T &addTarget(SwCoreContext &s, const PackageId &id)
{
    auto t = std::make_shared<T>(id);

    auto &cld = s.getTargets();
    cld[id].push_back(t);

    //t.sw_provided = true;
    return *t;
}

template <class T = PredefinedProgramTarget>
static T &addProgramNoFile(SwCoreContext &s, const PackageId &id, const std::shared_ptr<Program> &p)
{
    auto &t = addTarget<T>(s, id);
    t.setProgram(p);
    return t;
}

template <class T = PredefinedProgramTarget>
static T &addProgram(SwCoreContext &s, const PackageId &id, const std::shared_ptr<Program> &p)
{
    //if (!fs::exists(p->file))
        //throw SW_RUNTIME_ERROR("Program does not exist: " + normalize_path(p->file));
    return addProgramNoFile(s, id, p);
}

void detectDCompilers(SwCoreContext &s)
{
    SW_UNIMPLEMENTED;

    /*path compiler;
    compiler = resolveExecutable("dmd");
    if (compiler.empty())
        return;

    auto C = std::make_shared<DCompiler>(s.swctx);
    C->file = compiler;
    C->Extension = s.getBuildSettings().TargetOS.getExecutableExtension();
    //C->input_extensions = { ".d" };
    addProgram(s, "org.dlang.dmd.dmd", C);*/
}

void detectKotlinCompilers(SwCoreContext &s)
{
    SW_UNIMPLEMENTED;

    /*path compiler;
    compiler = resolveExecutable("kotlinc");
    if (compiler.empty())
        return;

    auto C = std::make_shared<KotlinCompiler>(s.swctx);
    C->file = compiler;
    //C->input_extensions = { ".kt", ".kts" };
    //s.registerProgram("com.JetBrains.kotlin.kotlinc", C);*/
}

void detectJavaCompilers(SwCoreContext &s)
{
    SW_UNIMPLEMENTED;

    /*path compiler;
    compiler = resolveExecutable("javac");
    if (compiler.empty())
        return;
    //compiler = resolveExecutable("jar"); // later

    auto C = std::make_shared<JavaCompiler>(s.swctx);
    C->file = compiler;
    //C->input_extensions = { ".java", };
    //s.registerProgram("com.oracle.java.javac", C);*/
}

void detectFortranCompilers(SwCoreContext &s)
{
    SW_UNIMPLEMENTED;

    /*path compiler;
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
    //s.registerProgram("org.gnu.gcc.fortran", C);*/
}

void detectGoCompilers(SwCoreContext &s)
{
    SW_UNIMPLEMENTED;

#if defined(_WIN32)
    /*auto compiler = path("go");
    compiler = resolveExecutable(compiler);
    if (compiler.empty())
        return;

    auto C = std::make_shared<GoCompiler>(s.swctx);
    C->file = compiler;
    SW_UNIMPLEMENTED;
    //C->Extension = s.Settings.TargetOS.getExecutableExtension();
    //C->input_extensions = { ".go", };
    //s.registerProgram("org.google.golang.go", C);*/
#else
#endif
}

void detectRustCompilers(SwCoreContext &s)
{
    SW_UNIMPLEMENTED;

#if defined(_WIN32)
    /*auto compiler = get_home_directory() / ".cargo" / "bin" / "rustc";
    compiler = resolveExecutable(compiler);
    if (compiler.empty())
        return;

    auto C = std::make_shared<RustCompiler>(s.swctx);
    C->file = compiler;
    SW_UNIMPLEMENTED;
    //C->Extension = s.Settings.TargetOS.getExecutableExtension();
    //C->input_extensions = { ".rs", };
    //s.registerProgram("org.rust.rustc", C);*/
#else
#endif
}

struct VSInstance
{
    path root;
    Version version;
};

using VSInstances = VersionMap<VSInstance>;

struct SimpleProgram : Program
{
    using Program::Program;

    std::shared_ptr<Program> clone() const override { return std::make_shared<SimpleProgram>(*this); }
    std::shared_ptr<builder::Command> getCommand() const override
    {
        auto c = std::make_shared<builder::Command>(swctx);
        c->setProgram(file);
        return c;
    }
};

VSInstances &gatherVSInstances(SwCoreContext &s)
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

void detectCSharpCompilers(SwCoreContext &s)
{
    SW_UNIMPLEMENTED;

    /*auto &instances = gatherVSInstances(s);
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
    }*/
}

void detectWindowsCompilers(SwCoreContext &s)
{
    // we need ifdef because of cmVSSetupAPIHelper
    // but what if we're on Wine?
    // reconsider later

    // https://docs.microsoft.com/en-us/cpp/c-runtime-library/crt-library-features?view=vs-2019

    auto &instances = gatherVSInstances(s);
    const auto host = toStringWindows(s.getHostOs().Arch);
    OsSdk sdk(s.getHostOs());
    auto new_settings = s.getHostOs();

    for (auto target_arch : {ArchType::x86_64,ArchType::x86,ArchType::arm,ArchType::aarch64})
    {
        new_settings.Arch = target_arch;

        auto ts1 = toTargetSettings(new_settings);
        TargetSettings ts;
        ts["os.kernel"] = ts1["os.kernel"];
        ts["os.arch"] = ts1["os.arch"];

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
                compiler /= path("Host" + host) / target;
            }
            // but we won't detect host&arch stuff on older versions

            // lib, link
            {
                auto p = std::make_shared<SimpleProgram>(s);
                p->file = compiler / "link.exe";
                if (fs::exists(p->file))
                {
                    Version v = getVersion(s, p->file, {});
                    if (instance.version.isPreRelease())
                        v.getExtra() = instance.version.getExtra();
                    auto &link = addProgram(s, PackageId("com.Microsoft.VisualStudio.VC.link", v), p);
                    link.ts = ts;
                }

                if (s.getHostOs().Arch != target_arch)
                {
                    auto c = p->getCommand();
                    c->addPathDirectory(host_root);
                }

                p = std::make_shared<SimpleProgram>(s);
                p->file = compiler / "lib.exe";
                if (fs::exists(p->file))
                {
                    Version v = getVersion(s, p->file, {});
                    if (instance.version.isPreRelease())
                        v.getExtra() = instance.version.getExtra();
                    auto &lib = addProgram(s, PackageId("com.Microsoft.VisualStudio.VC.lib", v), p);
                    lib.ts = ts;
                }

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
                    Version v = getVersion(s, p->file, {});
                    if (instance.version.isPreRelease())
                        v.getExtra() = instance.version.getExtra();
                    auto &ml = addProgram(s, PackageId("com.Microsoft.VisualStudio.VC.ml", v), p);
                    ml.ts = ts;
                }
            }

            // C, C++
            {
                auto p = std::make_shared<SimpleProgram>(s);
                p->file = compiler / "cl.exe";
                if (fs::exists(p->file))
                {
                    Version v = getVersion(s, p->file, {});
                    if (instance.version.isPreRelease())
                        v.getExtra() = instance.version.getExtra();
                    auto &cl = addProgram(s, PackageId("com.Microsoft.VisualStudio.VC.cl", v), p);
                    cl.ts = ts;
                }

                if (s.getHostOs().Arch != target_arch)
                {
                    auto c = p->getCommand();
                    c->addPathDirectory(host_root);
                }
            }

            // libs
            {
                auto &libcpp = addTarget<PredefinedTarget>(s, PackageId("com.Microsoft.VisualStudio.VC.libcpp", v));
                libcpp.ts = ts;
                libcpp.public_ts["system-include-directories"].push_back(normalize_path(root / "include"));

                if (v.getMajor() >= 15)
                {
                    libcpp.public_ts["system-link-directories"].push_back(normalize_path(root / "lib" / target));
                }
                else
                {
                    SW_UNIMPLEMENTED;
                }

                if (fs::exists(root / "ATLMFC" / "include"))
                {
                    auto &atlmfc = addTarget<PredefinedTarget>(s, PackageId("com.Microsoft.VisualStudio.VC.ATLMFC", v));
                    atlmfc.ts = ts;
                    atlmfc.public_ts["system-include-directories"].push_back(normalize_path(root / "ATLMFC" / "include"));

                    if (v.getMajor() >= 15)
                    {
                        atlmfc.public_ts["system-link-directories"].push_back(normalize_path(root / "ATLMFC" / "lib" / target));
                    }
                    else
                    {
                        SW_UNIMPLEMENTED;
                    }
                }
            }

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

        // rename to libc? to crt?
        auto &ucrt = addTarget<PredefinedTarget>(s, PackageId("com.Microsoft.Windows.SDK.ucrt", sdk.getWindowsTargetPlatformVersion()));
        ucrt.ts = ts;
        ucrt.ts["os.version"] = ts1["os.version"];

        // add kits include dirs
        for (auto &i : fs::directory_iterator(sdk.getPath("Include")))
        {
            if (fs::is_directory(i))
                ucrt.public_ts["system-include-directories"].push_back(normalize_path(i));
        }
        for (auto &i : fs::directory_iterator(sdk.getPath("Lib")))
        {
            if (fs::is_directory(i))
                ucrt.public_ts["system-link-directories"].push_back(normalize_path(i / toStringWindows(target_arch)));
        }
    }

    // .rc
    {
        auto p = std::make_shared<SimpleProgram>(s);
        p->file = sdk.getPath("bin") / toStringWindows(s.getHostOs().Arch) / "rc.exe";
        if (fs::exists(p->file))
        {
            Version v = getVersion(s, p->file, "/?");
            auto &rc = addProgram(s, PackageId("com.Microsoft.Windows.rc", v), p);
            auto ts1 = toTargetSettings(new_settings);
            rc.ts["os.kernel"] = ts1["os.kernel"];
        }
        //for (auto &idir : COpts.System.IncludeDirectories)
            //C->system_idirs.push_back(idir);
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

void detectNonWindowsCompilers(SwCoreContext &s)
{
    path p;

    SW_UNIMPLEMENTED;

    /*NativeLinkerOptions LOpts;

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
    }*/

    //p = resolve("ld.gold");
    //for (auto &v : gcc_vers)
    //for (auto &v : gccpp_vers) // this links correct c++ library
    {
        SW_UNIMPLEMENTED;

        /*p = resolve(v);
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
        }*/
    }

    //NativeCompilerOptions COpts;

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

    /*for (auto &v : gcc_vers)
    {
        p = resolve(v);
        if (!p.empty())
        {
            SW_UNIMPLEMENTED;

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
            SW_UNIMPLEMENTED;

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
    }*/

    // llvm/clang
    {
        SW_UNIMPLEMENTED;
        //p = resolve("llvm-ar");
        if (!p.empty())
        {
            SW_UNIMPLEMENTED;

            /*auto Librarian = std::make_shared<GNULibrarian>(s.swctx);
            Librarian->Type = LinkerType::GNU;
            Librarian->file = p;
            SW_UNIMPLEMENTED;
            //Librarian->Prefix = s.Settings.TargetOS.getLibraryPrefix();
            //Librarian->Extension = s.Settings.TargetOS.getStaticLibraryExtension();
            *Librarian = LOpts;
            //s.registerProgram("org.LLVM.ar", Librarian);
            //if (s.getHostOs().is(OSType::Macos))
            //Librarian->createCommand()->use_response_files = false;*/
        }

        ////p = resolve("ld.gold");
        SW_UNIMPLEMENTED;
        /*for (auto &v : clang_vers)
        //for (auto &v : clangpp_vers) // this links correct c++ library
        {
            p = resolve(v);
            if (!p.empty())
            {
                SW_UNIMPLEMENTED;

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
        }*/

        SW_UNIMPLEMENTED;
        /*for (auto &v : clangpp_vers)
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
        }*/
    }
}

void detectNativeCompilers(SwCoreContext &s)
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
}

}
