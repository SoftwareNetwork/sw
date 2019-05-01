// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "solution_build.h"

#include "frontend/cppan/yaml.h"
#include "functions.h"
#include "generator/generator.h"
#include "inserts.h"
#include "module.h"
#include "run.h"
#include "solution_build.h"
#include "sw_abi_version.h"
#include "target/native.h"

#include <sw/builder/execution_plan.h>
#include <sw/builder/file_storage.h>
#include <sw/builder/program.h>
#include <sw/builder/sw_context.h>
#include <sw/manager/database.h>
#include <sw/manager/settings.h>
#include <sw/manager/storage.h>

#include <boost/algorithm/string.hpp>
#include <boost/range/combine.hpp>
#include <boost/thread/lock_types.hpp>
#include <primitives/emitter.h>
#include <primitives/date_time.h>
#include <primitives/executor.h>
#include <primitives/pack.h>
#include <primitives/symbol.h>
#include <primitives/templates.h>
#include <primitives/win32helpers.h>
#include <primitives/sw/cl.h>
#include <primitives/sw/settings_program_name.h>

#include <boost/dll.hpp>
#include <nlohmann/json.hpp>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "build");

static cl::opt<bool> append_configs("append-configs", cl::desc("Append configs for generation"));
String gGenerator;
static cl::opt<bool> do_not_rebuild_config("do-not-rebuild-config", cl::Hidden);
cl::opt<bool> dry_run("n", cl::desc("Dry run"));
static cl::opt<bool> debug_configs("debug-configs", cl::desc("Build configs in debug mode"));
static cl::opt<bool> fetch_sources("fetch", cl::desc("Fetch files in process"));

static cl::opt<int> config_jobs("jc", cl::desc("Number of config jobs"));

static cl::list<String> libc("libc", cl::CommaSeparated);
//static cl::list<String> libcpp("libcpp", cl::CommaSeparated);
static cl::list<String> target_os("target-os", cl::CommaSeparated);
static cl::list<String> compiler("compiler", cl::desc("Set compiler"), cl::CommaSeparated);
static cl::list<String> configuration("configuration", cl::desc("Set build configuration"), cl::CommaSeparated);
cl::alias configuration2("config", cl::desc("Alias for -configuration"), cl::aliasopt(configuration));
static cl::list<String> platform("platform", cl::desc("Set build platform"), cl::CommaSeparated);
//static cl::opt<String> arch("arch", cl::desc("Set arch")/*, cl::sub(subcommand_ide)*/);

// simple -static, -shared?
static cl::opt<bool> static_build("static-build", cl::desc("Set static build"));
cl::alias static_build2("static", cl::desc("Alias for -static-build"), cl::aliasopt(static_build));
static cl::opt<bool> shared_build("shared-build", cl::desc("Set shared build (default)"));
cl::alias shared_build2("shared", cl::desc("Alias for -shared-build"), cl::aliasopt(shared_build));

// simple -mt, -md?
static cl::opt<bool> win_mt("win-mt", cl::desc("Set /MT build"));
cl::alias win_mt2("mt", cl::desc("Alias for -win-mt"), cl::aliasopt(win_mt));
static cl::opt<bool> win_md("win-md", cl::desc("Set /MD build (default)"));
cl::alias win_md2("md", cl::desc("Alias for -win-md"), cl::aliasopt(win_md));

//static cl::opt<bool> hide_output("hide-output");
static cl::opt<bool> cl_show_output("show-output");

extern bool gVerbose;
bool gWithTesting;
path gIdeFastPath;
path gIdeCopyToDir;
int gNumberOfJobs = -1;

// TODO: add '#pragma sw driver ...'

namespace sw
{

void build_self(sw::Solution &s);
void check_self(sw::Checker &c);

static String getCurrentModuleId()
{
    return shorten_hash(sha1(getProgramName()), 6);
}

static path getImportFilePrefix(const SwContext &swctx)
{
    return swctx.getLocalStorage().storage_dir_tmp / ("sw_" + getCurrentModuleId());
}

static path getImportDefinitionsFile(const SwContext &swctx)
{
    return getImportFilePrefix(swctx) += ".def";
}

static path getImportLibraryFile(const SwContext &swctx)
{
    return getImportFilePrefix(swctx) += ".lib";
}

static path getImportPchFile(const SwContext &swctx)
{
    return getImportFilePrefix(swctx) += ".cpp";
}

static void addImportLibrary(const SwContext &swctx, NativeExecutedTarget &t)
{
#if defined(CPPAN_OS_WINDOWS)
    HMODULE lib = (HMODULE)primitives::getModuleForSymbol();
    PIMAGE_NT_HEADERS header = (PIMAGE_NT_HEADERS)((BYTE *)lib + ((PIMAGE_DOS_HEADER)lib)->e_lfanew);
    PIMAGE_EXPORT_DIRECTORY exports = (PIMAGE_EXPORT_DIRECTORY)((BYTE *)lib + header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
    assert(exports->AddressOfNames && "No exports found");
    int* names = (int*)((uint64_t)lib + exports->AddressOfNames);
    String defs;
    defs += "LIBRARY " IMPORT_LIBRARY "\n";
    defs += "EXPORTS\n";
    for (DWORD i = 0; i < exports->NumberOfNames; i++)
    {
        const char *n = (const char *)lib + names[i];
        defs += "    "s + n + "\n";
    }
    write_file_if_different(getImportDefinitionsFile(swctx), defs);

    auto c = t.addCommand();
    c.c->working_directory = getImportDefinitionsFile(swctx).parent_path();
    c << t.Librarian->file
        << cmd::in(getImportDefinitionsFile(swctx), cmd::Prefix{ "-DEF:" }, cmd::Skip)
        << cmd::out(getImportLibraryFile(swctx), cmd::Prefix{ "-OUT:" })
        ;
    t.LinkLibraries.push_back(getImportLibraryFile(swctx));
#endif
}

static path getPackageHeader(const LocalPackage &p, const UnresolvedPackage &up)
{
    // depends on upkg, not on pkg!
    // because p is constant, but up might differ
    auto h = p.getDirSrc() / "gen" / ("pkg_header_" + shorten_hash(sha1(up.toString()), 6) + ".h");
    //if (fs::exists(h))
    //return h;
    auto cfg = p.getDirSrc2() / "sw.cpp";
    auto f = read_file(cfg);
    std::smatch m;
    // replace with while?
    const char on[] = "#pragma sw header on";
    auto pos = f.find(on);
    if (pos == f.npos)
        throw SW_RUNTIME_ERROR("No header for package: " + p.toString());
    f = f.substr(pos + sizeof(on));
    pos = f.find("#pragma sw header off");
    if (pos == f.npos)
        throw SW_RUNTIME_ERROR("No end in header for package: " + p.toString());
    f = f.substr(0, pos);
    //static const std::regex r_header("#pragma sw header on(.*)#pragma sw header off");
    //if (std::regex_search(f, m, r_header))
    {
        primitives::Emitter ctx;
        ctx.addLine("#pragma once");
        ctx.addLine();
        //ctx.addLine("#line 1 \"" + normalize_path(cfg) + "\""); // determine correct line number first

        primitives::Emitter prefix;
        /*prefix.addLine("#define THIS_PREFIX \"" + p.ppath.slice(0, p.prefix).toString() + "\"");
        prefix.addLine("#define THIS_RELATIVE_PACKAGE_PATH \"" + p.ppath.slice(p.prefix).toString() + "\"");
        prefix.addLine("#define THIS_PACKAGE_PATH THIS_PREFIX \".\" THIS_RELATIVE_PACKAGE_PATH");
        //prefix.addLine("#define THIS_VERSION \"" + p.version.toString() + "\"");
        //prefix.addLine("#define THIS_VERSION_DEPENDENCY \"" + p.version.toString() + "\"_dep");
        prefix.addLine("#define THIS_VERSION_DEPENDENCY \"" + up.range.toString() + "\"_dep"); // here we use range! our packages must depend on exactly specified range
        //prefix.addLine("#define THIS_PACKAGE THIS_PACKAGE_PATH \"-\" THIS_VERSION");
        prefix.addLine("#define THIS_PACKAGE_DEPENDENCY THIS_PACKAGE_PATH \"-\" THIS_VERSION_DEPENDENCY");
        prefix.addLine();*/

        auto ins_pre = "#pragma sw header insert prefix";
        if (f.find(ins_pre) != f.npos)
            boost::replace_all(f, ins_pre, prefix.getText());
        else
            ctx += prefix;

        ctx.addLine(f);
        ctx.addLine();

        /*ctx.addLine("#undef THIS_PREFIX");
        ctx.addLine("#undef THIS_RELATIVE_PACKAGE_PATH");
        ctx.addLine("#undef THIS_PACKAGE_PATH");
        ctx.addLine("#undef THIS_VERSION");
        ctx.addLine("#undef THIS_VERSION_DEPENDENCY");
        ctx.addLine("#undef THIS_PACKAGE");
        ctx.addLine("#undef THIS_PACKAGE_DEPENDENCY");
        ctx.addLine();*/

        write_file_if_different(h, ctx.getText());
    }
    return h;
}

static std::tuple<FilesOrdered, UnresolvedPackages> getFileDependencies(const SwContext &swctx, const path &p)
{
    UnresolvedPackages udeps;
    FilesOrdered headers;

    auto f = read_file(p);
#ifdef _WIN32
    static const std::regex r_pragma("^#pragma +sw +require +(\\S+)( +(\\S+))?");
#else
    static const std::regex r_pragma("#pragma +sw +require +(\\S+)( +(\\S+))?");
#endif
    std::smatch m;
    while (std::regex_search(f, m, r_pragma))
    {
        auto m1 = m[1].str();
        if (m1 == "header")
        {
            auto upkg = extractFromString(m[3].str());
            auto pkg = swctx.resolve(upkg);
            auto h = getPackageHeader(pkg, upkg);
            auto [headers2,udeps2] = getFileDependencies(swctx, h);
            headers.insert(headers.end(), headers2.begin(), headers2.end());
            udeps.insert(udeps2.begin(), udeps2.end());
            headers.push_back(h);
        }
        else if (m1 == "local")
        {
            auto [headers2, udeps2] = getFileDependencies(swctx, m[3].str());
            headers.insert(headers.end(), headers2.begin(), headers2.end());
            udeps.insert(udeps2.begin(), udeps2.end());
        }
        else
            udeps.insert(extractFromString(m1));
        f = m.suffix().str();
    }

    return { headers, udeps };
}

path build_configs(const SwContext &swctx, const std::unordered_set<LocalPackage> &pkgs)
{
    //static
    Build b(swctx); // cache?
    b.execute_jobs = config_jobs;
    b.Local = false;
    b.file_storage_local = false;
    b.is_config_build = true;
    return b.build_configs(pkgs);
}

void sw_check_abi_version(int v)
{
    if (v > SW_MODULE_ABI_VERSION)
        throw SW_RUNTIME_ERROR("Module ABI (" + std::to_string(v) + ") is greater than binary ABI (" + std::to_string(SW_MODULE_ABI_VERSION) + "). Update your sw binary.");
    if (v < SW_MODULE_ABI_VERSION)
        throw SW_RUNTIME_ERROR("Module ABI (" + std::to_string(v) + ") is less than binary ABI (" + std::to_string(SW_MODULE_ABI_VERSION) + "). Update sw driver headers (or ask driver maintainer).");
}

Build::Build(const SwContext &swctx)
    : Solution(swctx)
{
    HostOS = swctx.HostOS;
    Settings.TargetOS = HostOS; // default

    // load service local fs by default
    fs = &swctx.getServiceFileStorage();
}

Build::~Build()
{
    // first destroy children as they might have data references to modules
    solutions.clear();

    // clear this solution before modules
    // (events etc.)
    clear();

    // maybe also clear checks?
    // or are they solution-specific?

    // do not clear modules on exception, because it may come from there
    if (!std::uncaught_exceptions())
        getModuleStorage(base_ptr).modules.clear();
}

Solution::CommandExecutionPlan Build::getExecutionPlan() const
{
    Commands cmds;
    for (auto &s : solutions)
    {
        // if we added host solution, but did not select any targets from it, drop it
        // otherwise getCommands() will select all targets
        if (getHostSolution() == &s && s.TargetsToBuild.empty())
            continue;
        auto c = s.getCommands();
        cmds.insert(c.begin(), c.end());
    }
    return Solution::getExecutionPlan(cmds);
}

void Build::performChecks()
{
    LOG_DEBUG(logger, "Performing checks");

    ScopedTime t;

    auto &e = getExecutor();
    std::vector<Future<void>> fs;
    for (auto &s : solutions)
        fs.push_back(e.push([&s] { s.performChecks(); }, solutions.size()));
    waitAndGet(fs);

    if (!silent)
        LOG_DEBUG(logger, "Checks time: " << t.getTimeFloat() << " s.");
}

void Build::prepare()
{
    //if (prepared)
        //return;

    if (solutions.empty())
        throw SW_RUNTIME_ERROR("no solutions");

    ScopedTime t;

    // all targets are set stay unchanged from user
    // so, we're ready to some preparation passes

    for (const auto &[i, s] : enumerate(solutions))
    {
        if (solutions.size() > 1)
            LOG_INFO(logger, "[" << (i + 1) << "/" << solutions.size() << "] resolve deps pass " << s.getConfig());
        s.build_and_resolve();
    }

    // resolve all deps first
    /*auto &e = getExecutor();
    Futures<void> fs;
    for (const auto &[i, s] : enumerate(solutions))
    {
        //if (solutions.size() > 1)
            //LOG_INFO(logger, "[" << (i + 1) << "/" << solutions.size() << "] resolve deps pass " << s.getConfig());
        fs.push_back(e.push([&s] {s.build_and_resolve(); }));
    }
    waitAndGet(fs);*/

    // decide if we need cross compilation

    // multipass prepare()
    // if we add targets inside this loop,
    // it will automatically handle this situation
    while (prepareStep())
        ;

    // prepare tests
    if (with_testing)
    {
        for (auto &s : solutions)
        {
            for (auto &t : s.tests)
                ;
        }
    }

    //prepared = true;

    // prevent memory leaks (high mem usage)
    //updateConcurrentContext();

    if (!silent)
        LOG_DEBUG(logger, "Prepare time: " << t.getTimeFloat() << " s.");
}

// multi-solution, for crosscompilation
bool Build::prepareStep()
{
    std::atomic_bool next_pass = false;

    auto &e = getExecutor();
    Futures<void> fs;
    for (auto &s : solutions)
        s.prepareStep(e, fs, next_pass, getHostSolution());
    waitAndGet(fs);

    return next_pass;
}

Solution &Build::addSolutionRaw()
{
    auto &s = solutions.emplace_back(*this);
    s.build = this;
    return s;
}

Solution &Build::addSolution()
{
    auto &s = addSolutionRaw();
    s.findCompiler(); // too early?
    return s;
}

Solution &Build::addCustomSolution()
{
    auto &s = addSolutionRaw();
    s.prepareForCustomToolchain();
    return s;
}

std::optional<std::reference_wrapper<Solution>> Build::addFirstSolution()
{
    if (solutions.empty())
        return addSolution();
    return solutions[0];
}

static auto getFilesHash(const Files &files)
{
    String h;
    for (auto &fn : files)
        h += fn.u8string();
    return shorten_hash(blake2b_512(h), 6);
}

PackagePath Build::getSelfTargetName(const Files &files)
{
    return "loc.sw.self." + getFilesHash(files);
}

SharedLibraryTarget &Build::createTarget(const Files &files)
{
    auto &solution = solutions[0];
    solution.IsConfig = true;
    auto &lib = solution.addTarget<SharedLibraryTarget>(getSelfTargetName(files), "local");
    solution.IsConfig = false;
    return lib;
}

// TODO: remove '.cpp' part later
#define SW_DRIVER_NAME "org.sw.sw.client.driver.cpp"
#define SW_DRIVER_INCLUDE_DIR "src"

static void addDeps(NativeExecutedTarget &lib, Solution &solution)
{
    //lib += solution.getTarget<NativeTarget>("pub.egorpugin.primitives.version");
    lib += solution.getTarget<NativeTarget>("pub.egorpugin.primitives.templates"); // for SW_RUNTIME_ERROR

    auto &drv = solution.getTarget<NativeTarget>(SW_DRIVER_NAME);
    auto d = lib + drv;
    d->IncludeDirectoriesOnly = true;

    // generated file
    lib += drv.BinaryDir / "options_cl.generated.h";
}

// add Dirs?
static path getDriverIncludeDir(Solution &solution)
{
    return solution.getTarget<NativeTarget>(SW_DRIVER_NAME).SourceDir / SW_DRIVER_INCLUDE_DIR;
}

static path getMainPchFilename()
{
    return path("sw") / "driver" / "sw.h";
}

static path getSwHeader()
{
    return getMainPchFilename();
}

static path getSw1Header()
{
    return path("sw") / "driver" / "sw1.h";
}

static path getSwCheckAbiVersionHeader()
{
    return path("sw") / "driver" / "sw_check_abi_version.h";
}

static void write_pch(Solution &solution)
{
    write_file_if_different(getImportPchFile(solution.swctx),
        //"#include <" + normalize_path(getDriverIncludeDir(solution) / getMainPchFilename()) + ">\n\n" +
        //"#include <" + getDriverIncludePathString(solution, getMainPchFilename()) + ">\n\n" +
        //"#include <" + normalize_path(getMainPchFilename()) + ">\n\n" + // the last one
        cppan_cpp);
}

path Build::getOutputModuleName(const path &p)
{
    addFirstSolution();

    auto &solution = solutions[0];

    solution.Settings.Native.LibrariesType = LibraryType::Static;
    if (debug_configs)
        solution.Settings.Native.ConfigurationType = ConfigurationType::Debug;
    auto &lib = createTarget({ p });
    return lib.getOutputFile();
}

FilesMap Build::build_configs_separate(const Files &files)
{
    FilesMap r;
    if (files.empty())
        return r;

    addFirstSolution();

    auto &solution = solutions[0];

    solution.Settings.Native.LibrariesType = LibraryType::Static;
    if (debug_configs)
        solution.Settings.Native.ConfigurationType = ConfigurationType::Debug;

    bool once = false;
    auto prepare_config = [this, &once, &solution](const auto &fn)
    {
        auto &lib = createTarget({ fn });

        if (do_not_rebuild_config && fs::exists(lib.getOutputFile()))
            return lib.getOutputFile();

        do_not_rebuild_config = false;

        if (!once)
        {
            check_self(solution.checker);
            solution.performChecks();
            build_self(solution);
            addDeps(lib, solution);
            once = true;
        }

        addImportLibrary(swctx, lib);
        lib.AutoDetectOptions = false;
        lib.CPPVersion = CPPLanguageStandard::CPP17;

        lib += fn;
        write_pch(solution);
        PrecompiledHeader pch;
        pch.header = getDriverIncludeDir(solution) / getMainPchFilename();
        pch.source = getImportPchFile(swctx);
        pch.force_include_pch = true;
        pch.force_include_pch_to_source = true;
        lib.addPrecompiledHeader(pch);

        auto [headers, udeps] = getFileDependencies(swctx, fn);

        for (auto &h : headers)
        {
            // TODO: refactor this and same cases below
            if (auto sf = lib[fn].template as<NativeSourceFile>())
            {
                if (auto c = sf->compiler->template as<VisualStudioCompiler>())
                {
                    c->ForcedIncludeFiles().push_back(h);
                }
                else if (auto c = sf->compiler->template as<ClangClCompiler>())
                {
                    c->ForcedIncludeFiles().push_back(h);
                }
                else if (auto c = sf->compiler->template as<ClangCompiler>())
                {
                    c->ForcedIncludeFiles().push_back(h);
                }
                else if (auto c = sf->compiler->template as<GNUCompiler>())
                {
                    c->ForcedIncludeFiles().push_back(h);
                }
            }
        }

        if (auto sf = lib[fn].template as<NativeSourceFile>())
        {
            if (auto c = sf->compiler->template as<VisualStudioCompiler>())
            {
                c->ForcedIncludeFiles().push_back(getDriverIncludeDir(solution) / getSwCheckAbiVersionHeader());
            }
            else if (auto c = sf->compiler->template as<ClangClCompiler>())
            {
                c->ForcedIncludeFiles().push_back(getDriverIncludeDir(solution) / getSwCheckAbiVersionHeader());
            }
            else if (auto c = sf->compiler->template as<ClangCompiler>())
            {
                //c->ForcedIncludeFiles().push_back(getDriverIncludeDir(solution) / getSw1Header());
                c->ForcedIncludeFiles().push_back(getDriverIncludeDir(solution) / getSwCheckAbiVersionHeader());
            }
            else if (auto c = sf->compiler->template as<GNUCompiler>())
            {
                //c->ForcedIncludeFiles().push_back(getDriverIncludeDir(solution) / getSw1Header());
                c->ForcedIncludeFiles().push_back(getDriverIncludeDir(solution) / getSwCheckAbiVersionHeader());
            }
        }

        if (solution.Settings.TargetOS.is(OSType::Windows))
        {
            lib.Definitions["SW_SUPPORT_API"] = "__declspec(dllimport)";
            lib.Definitions["SW_MANAGER_API"] = "__declspec(dllimport)";
            lib.Definitions["SW_BUILDER_API"] = "__declspec(dllimport)";
            lib.Definitions["SW_DRIVER_CPP_API"] = "__declspec(dllimport)";
            // do not use api name because we use C linkage
            lib.Definitions["SW_PACKAGE_API"] = "extern \"C\" __declspec(dllexport)";
        }
        else
        {
            lib.Definitions["SW_SUPPORT_API="];
            lib.Definitions["SW_MANAGER_API="];
            lib.Definitions["SW_BUILDER_API="];
            lib.Definitions["SW_DRIVER_CPP_API="];
            // do not use api name because we use C linkage
            lib.Definitions["SW_PACKAGE_API"] = "extern \"C\" __attribute__ ((visibility (\"default\")))";
        }

        if (solution.Settings.TargetOS.is(OSType::Windows))
            lib.NativeLinkerOptions::System.LinkLibraries.insert("Delayimp.lib");

        if (auto L = lib.Linker->template as<VisualStudioLinker>())
        {
            L->DelayLoadDlls().push_back(IMPORT_LIBRARY);
            //#ifdef CPPAN_DEBUG
            L->GenerateDebugInformation = vs::link::Debug::Full;
            //#endif
            L->Force = vs::ForceType::Multiple;
            L->IgnoreWarnings().insert(4006); // warning LNK4006: X already defined in Y; second definition ignored
            L->IgnoreWarnings().insert(4070); // warning LNK4070: /OUT:X.dll directive in .EXP differs from output filename 'Y.dll'; ignoring directive
            // cannot be ignored https://docs.microsoft.com/en-us/cpp/build/reference/ignore-ignore-specific-warnings?view=vs-2017
            //L->IgnoreWarnings().insert(4088); // warning LNK4088: image being generated due to /FORCE option; image may not run
        }

        for (auto &d : udeps)
            lib += std::make_shared<Dependency>(d);

        auto i = solution.children.find(lib.getPackage());
        if (i == solution.children.end())
            throw std::logic_error("config target not found");
        solution.TargetsToBuild[i->first] = i->second;

        return lib.getOutputFile();
    };

    for (auto &fn : files)
        r[fn] = prepare_config(fn);

    if (!do_not_rebuild_config)
    {
        Solution::execute();
    }

    return r;
}

path Build::build_configs(const std::unordered_set<LocalPackage> &pkgs)
{
    if (pkgs.empty())
        return {};

    bool init = false;
    if (solutions.empty())
    {
        addFirstSolution();

        auto &solution = solutions[0];

        solution.Settings.Native.LibrariesType = LibraryType::Static;
        if (debug_configs)
            solution.Settings.Native.ConfigurationType = ConfigurationType::Debug;

        init = true;
    }

    auto &solution = solutions[0];

    // make parallel?
    auto get_real_package = [](const auto &pkg) -> LocalPackage
    {
        if (pkg.getData().group_number)
            return pkg;
        auto p = pkg.getGroupLeader();
        if (fs::exists(p.getDirSrc2() / "sw.cpp"))
            return p;
        fs::create_directories(p.getDirSrc2());
        fs::copy_file(pkg.getDirSrc2() / "sw.cpp", p.getDirSrc2() / "sw.cpp");
        //resolve_dependencies({p}); // p might not be downloaded
        return p;
    };

    auto get_real_package_config = [&get_real_package](const auto &pkg)
    {
        return get_real_package(pkg).getDirSrc2() / "sw.cpp";
    };

    Files files;
    std::unordered_map<path, LocalPackage> output_names;
    for (auto &pkg : pkgs)
    {
        auto p = get_real_package_config(pkg);
        files.insert(p);
        output_names.emplace(p, pkg);
    }
    bool many_files = true;
    auto h = getFilesHash(files);

    auto &lib = createTarget(files);

    SCOPE_EXIT
    {
        solution.children.erase(lib.getPackage());
    };

    if (do_not_rebuild_config && fs::exists(lib.getOutputFile()))
        return lib.getOutputFile();

    do_not_rebuild_config = false;

    if (init)
    {
        check_self(solution.checker);
        solution.performChecks();
        build_self(solution);
    }
    addDeps(lib, solution);

    addImportLibrary(swctx, lib);
    lib.AutoDetectOptions = false;
    lib.CPPVersion = CPPLanguageStandard::CPP17;

    // separate loop
    for (auto &[fn, pkg] : output_names)
    {
        lib += fn;
        lib[fn].fancy_name = "[" + output_names.find(fn)->second.toString() + "]/[config]";
        // configs depend on pch, and pch depends on getCurrentModuleId(), so we add name to the file
        // to make sure we have different config .objs for different pchs
        lib[fn].as<NativeSourceFile>()->setOutputFile(lib, fn.u8string() + "." + getCurrentModuleId(), solution.getObjectDir(pkg) / "self");
        if (gVerbose)
            lib[fn].fancy_name += " (" + normalize_path(fn) + ")";
    }

    auto is_changed = [this, &lib](const path &p)
    {
        if (auto f = lib[p].as<NativeSourceFile>(); f)
        {
            return
                File(p, *fs).isChanged() ||
                File(f->compiler->getOutputFile(), *fs).isChanged()
                ;
        }
        return true;
    };

    // generate main source file
    path many_files_fn;
    if (many_files)
    {
        primitives::CppEmitter ctx;

        primitives::CppEmitter build;
        build.beginFunction("void build(Solution &s)");

        primitives::CppEmitter check;
        check.beginFunction("void check(Checker &c)");

        primitives::CppEmitter abi;
        abi.addLine("SW_PACKAGE_API");
        abi.beginFunction("int sw_get_module_abi_version()");
        abi.addLine("int v = -1, t;");
        abi.addLine("String current_module, prev_module;");
        abi.addLine();

        abi.beginBlock("auto check = [&t, &v, &current_module, &prev_module]()");
        abi.addLine("if (v == -1)");
        abi.increaseIndent();
        abi.addLine("v = t;");
        abi.decreaseIndent();
        abi.addLine("if (t != v)");
        abi.increaseIndent();
        abi.addLine("throw SW_RUNTIME_ERROR(\"ABI mismatch in loaded modules: previous "
            "(\" + std::to_string(v) + \", \" + prev_module + \") != current (\" + std::to_string(t) + \", \" + current_module + \")\");");
        abi.decreaseIndent();
        abi.addLine("prev_module = current_module;");
        abi.endBlock(true);
        abi.addLine();

        for (auto &r : pkgs)
        {
            auto fn = get_real_package_config(r);
            auto h = getFilesHash({ fn });
            ctx.addLine("// " + r.toString());
            ctx.addLine("// " + normalize_path(fn));
            if (HostOS.Type != OSType::Windows)
                ctx.addLine("extern \"C\"");
            ctx.addLine("void build_" + h + "(Solution &);");
            if (HostOS.Type != OSType::Windows)
                ctx.addLine("extern \"C\"");
            ctx.addLine("void check_" + h + "(Checker &);");
            ctx.addLine("SW_PACKAGE_API");
            ctx.addLine("int sw_get_module_abi_version_" + h + "();");
            ctx.addLine();

            build.addLine("// " + r.toString());
            build.addLine("// " + normalize_path(fn));
            build.addLine("s.NamePrefix = \"" + r.ppath.slice(0, r.getData().prefix).toString() + "\";");
            build.addLine("s.current_module = \"" + r.toString() + "\";");
            build.addLine("s.current_gn = " + std::to_string(r.getData().group_number) + ";");
            build.addLine("build_" + h + "(s);");
            build.addLine();

            abi.addLine("// " + r.toString());
            abi.addLine("// " + normalize_path(fn));
            abi.addLine("t = sw_get_module_abi_version_" + h + "();");
            abi.addLine("current_module = \"" + r.toString() + "\";");
            abi.addLine("check();");
            abi.addLine();

            auto cfg = read_file(fn);
            if (cfg.find("void check(") != cfg.npos)
            {
                check.addLine("// " + r.toString());
                check.addLine("c.current_gn = " + std::to_string(r.getData().group_number) + ";");
                check.addLine("check_" + h + "(c);");
                check.addLine();
            }
        }

        build.addLine("s.NamePrefix.clear();");
        build.addLine("s.current_module.clear();");
        build.addLine("s.current_gn = 0;");
        build.endFunction();
        check.addLine("c.current_gn = 0;");
        check.endFunction();
        abi.addLine("return v;");
        abi.endFunction();

        ctx += build;
        ctx += check;
        ctx += abi;

        auto p = many_files_fn = BinaryDir / "self" / ("sw." + h + ".cpp");
        write_file_if_different(p, ctx.getText());
        lib += p;
        lib[p].fancy_name = "[multiconfig]";
        if (gVerbose)
            lib[p].fancy_name += " (" + normalize_path(p) + ")";

        /*if (!(is_changed(p) ||
              File(lib.getOutputFile(), *fs).isChanged() ||
              std::any_of(files.begin(), files.end(), [&is_changed](const auto &p) {
                  return is_changed(p);
              })))
            return lib.getOutputFile();*/
    }
    /*else if (!(is_changed(*files.begin()) ||
               File(lib.getOutputFile(), *fs).isChanged()))
        return lib.getOutputFile();*/

    // after files
    write_pch(solution);
    PrecompiledHeader pch;
    pch.header = getDriverIncludeDir(solution) / getMainPchFilename();
    pch.source = getImportPchFile(swctx);
    pch.force_include_pch = true;
    pch.force_include_pch_to_source = true;
    lib.addPrecompiledHeader(pch);

    auto gnu_setup = [this, &solution](auto *c, const auto &headers, const path &fn)
    {
        // we use pch, but cannot add more defs on CL
        // so we create a file with them
        auto hash = getFilesHash({ fn });
        path h;
        // cannot create aux dir on windows; auxl = auxiliary
        if (is_under_root(fn, swctx.getLocalStorage().storage_dir_pkg))
            h = fn.parent_path().parent_path() / "auxl" / ("defs_" + hash + ".h");
        else
            h = fn.parent_path() / SW_BINARY_DIR / "auxl" / ("defs_" + hash + ".h");
        primitives::CppEmitter ctx;

        ctx.addLine("#define configure configure_" + hash);
        ctx.addLine("#define build build_" + hash);
        ctx.addLine("#define check check_" + hash);
        ctx.addLine("#define sw_get_module_abi_version sw_get_module_abi_version_" + hash);

        write_file_if_different(h, ctx.getText());
        c->ForcedIncludeFiles().push_back(h);
        //c->ForcedIncludeFiles().push_back(getDriverIncludeDir(solution) / getSw1Header());

        for (auto &h : headers)
            c->ForcedIncludeFiles().push_back(h);
        c->ForcedIncludeFiles().push_back(getDriverIncludeDir(solution) / getSwCheckAbiVersionHeader());
    };

    for (auto &fn : files)
    {
        auto[headers, udeps] = getFileDependencies(swctx, fn);
        if (auto sf = lib[fn].template as<NativeSourceFile>())
        {
            auto add_defs = [&many_files, &fn](auto &c)
            {
                if (!many_files)
                    return;
                auto h = getFilesHash({ fn });
                c->Definitions["configure"] = "configure_" + h;
                c->Definitions["build"] = "build_" + h;
                c->Definitions["check"] = "check_" + h;
                c->Definitions["sw_get_module_abi_version"] = "sw_get_module_abi_version_" + h;
            };

            if (auto c = sf->compiler->template as<VisualStudioCompiler>())
            {
                add_defs(c);
                for (auto &h : headers)
                    c->ForcedIncludeFiles().push_back(h);
                c->ForcedIncludeFiles().push_back(getDriverIncludeDir(solution) / getSwCheckAbiVersionHeader());
            }
            else if (auto c = sf->compiler->template as<ClangClCompiler>())
            {
                add_defs(c);
                for (auto &h : headers)
                    c->ForcedIncludeFiles().push_back(h);
                c->ForcedIncludeFiles().push_back(getDriverIncludeDir(solution) / getSwCheckAbiVersionHeader());
            }
            else if (auto c = sf->compiler->template as<ClangCompiler>())
            {
                gnu_setup(c, headers, fn);
            }
            else if (auto c = sf->compiler->template as<GNUCompiler>())
            {
                gnu_setup(c, headers, fn);
            }
        }
        for (auto &d : udeps)
            lib += std::make_shared<Dependency>(d);
    }

    /*if (many_files)
    {
        if (auto sf = lib[many_files_fn].template as<NativeSourceFile>())
        {
            if (auto c = sf->compiler->template as<ClangCompiler>())
            {
                c->ForcedIncludeFiles().push_back(getDriverIncludeDir(solution) / getSw1Header());
            }
            else if (auto c = sf->compiler->template as<GNUCompiler>())
            {
                c->ForcedIncludeFiles().push_back(getDriverIncludeDir(solution) / getSw1Header());
            }
        }
    }*/

    if (solution.Settings.TargetOS.is(OSType::Windows))
    {
        lib.Definitions["SW_SUPPORT_API"] = "__declspec(dllimport)";
        lib.Definitions["SW_MANAGER_API"] = "__declspec(dllimport)";
        lib.Definitions["SW_BUILDER_API"] = "__declspec(dllimport)";
        lib.Definitions["SW_DRIVER_CPP_API"] = "__declspec(dllimport)";
        // do not use api name because we use C linkage
        lib.Definitions["SW_PACKAGE_API"] = "extern \"C\" __declspec(dllexport)";
    }
    else
    {
        lib.Definitions["SW_SUPPORT_API="];
        lib.Definitions["SW_MANAGER_API="];
        lib.Definitions["SW_BUILDER_API="];
        lib.Definitions["SW_DRIVER_CPP_API="];
        // do not use api name because we use C linkage
        lib.Definitions["SW_PACKAGE_API"] = "extern \"C\" __attribute__ ((visibility (\"default\")))";
    }

    if (solution.Settings.TargetOS.is(OSType::Windows))
        lib.NativeLinkerOptions::System.LinkLibraries.insert("Delayimp.lib");

    if (auto L = lib.Linker->template as<VisualStudioLinker>())
    {
        L->DelayLoadDlls().push_back(IMPORT_LIBRARY);
        //#ifdef CPPAN_DEBUG
        L->GenerateDebugInformation = vs::link::Debug::Full;
        //#endif
        L->Force = vs::ForceType::Multiple;
        L->IgnoreWarnings().insert(4006); // warning LNK4006: X already defined in Y; second definition ignored
        L->IgnoreWarnings().insert(4070); // warning LNK4070: /OUT:X.dll directive in .EXP differs from output filename 'Y.dll'; ignoring directive
        // cannot be ignored https://docs.microsoft.com/en-us/cpp/build/reference/ignore-ignore-specific-warnings?view=vs-2017
        //L->IgnoreWarnings().insert(4088); // warning LNK4088: image being generated due to /FORCE option; image may not run
    }

    auto i = solution.children.find(lib.getPackage());
    if (i == solution.children.end())
        throw std::logic_error("config target not found");
    solution.TargetsToBuild[i->first] = i->second;

    Solution::execute();

    return lib.getOutputFile();
}

// can be used in configs to load subdir configs
// s.build->loadModule("client/sw.cpp").call<void(Solution &)>("build", s);
const Module &Build::loadModule(const path &p) const
{
    auto fn2 = p;
    if (!fn2.is_absolute())
        fn2 = SourceDir / fn2;

    Build b(swctx);
    b.execute_jobs = config_jobs;
    b.file_storage_local = false;
    b.is_config_build = true;
    path dll;
    //dll = b.getOutputModuleName(fn2);
    //if (File(fn2, *b.solutions[0].fs).isChanged() || File(dll, *b.solutions[0].fs).isChanged())
    {
        auto r = b.build_configs_separate({ fn2 });
        dll = r.begin()->second;
    }
    return getModuleStorage(base_ptr).get(dll);
}

path Build::build(const path &fn)
{
    if (fs::is_directory(fn))
        throw SW_RUNTIME_ERROR("Filename expected");

    auto fe = selectFrontendByFilename(fn);
    if (!fe)
        throw SW_RUNTIME_ERROR("Unknown frontend config: " + fn.u8string());

    setupSolutionName(fn);
    config = fn;

    switch (fe.value())
    {
    case FrontendType::Sw:
    {
        // separate build
        Build b(swctx);
        b.execute_jobs = config_jobs;
        b.file_storage_local = false;
        b.is_config_build = true;
        auto r = b.build_configs_separate({ fn });
        auto dll = r.begin()->second;
        if (do_not_rebuild_config &&
            (File(fn, *b.solutions[0].fs).isChanged() ||
                File(dll, *b.solutions[0].fs).isChanged()))
        {
            remove_ide_explans = true;
            do_not_rebuild_config = false;
            return build(fn);
        }
        return dll;
    }
    case FrontendType::Cppan:
        // no need to build
        break;
    }
    return {};
}

void Build::setupSolutionName(const path &file_or_dir)
{
    config_file_or_dir = fs::canonical(file_or_dir);

    bool dir = fs::is_directory(file_or_dir);
    if (dir || isFrontendConfigFilename(file_or_dir))
        ide_solution_name = fs::canonical(file_or_dir).parent_path().filename().u8string();
    else
        ide_solution_name = file_or_dir.stem().u8string();
}

void Build::load(const path &fn, bool configless)
{
    if (!fn.is_absolute())
        throw SW_RUNTIME_ERROR("path must be absolute: " + normalize_path(fn));
    if (!fs::exists(fn))
        throw SW_RUNTIME_ERROR("path does not exists: " + normalize_path(fn));

    if (!gGenerator.empty())
    {
        generator = Generator::create(gGenerator);

        // set early, before prepare

        // also add tests to solution
        // protect with option
        with_testing = true;
    }

    if (configless)
        return load_configless(fn);

    auto dll = build(fn);

    //fs->save(); // remove?
    //fs->reset();

    if (fetch_sources)
        fetch_dir = BinaryDir / "src";

    auto fe = selectFrontendByFilename(fn);
    if (!fe)
        throw SW_RUNTIME_ERROR("frontend was not found for file: " + normalize_path(fn));

    LOG_TRACE(logger, "using " << toString(*fe) << " frontend");
    switch (fe.value())
    {
    case FrontendType::Sw:
        load_dll(dll);
        break;
    case FrontendType::Cppan:
        cppan_load();
        break;
    }

    // set show output setting
    show_output = cl_show_output;
    for (auto &s : solutions)
        s.show_output = cl_show_output;
}

static Solution::CommandExecutionPlan load(const SwContext &swctx, const path &fn, const Solution &s)
{
    primitives::BinaryStream ctx;
    ctx.load(fn);

    size_t sz;
    ctx.read(sz);

    size_t n_strings;
    ctx.read(n_strings);

    Strings strings(1);
    while (n_strings--)
    {
        String s;
        ctx.read(s);
        strings.push_back(s);
    }

    auto read_string = [&strings, &ctx, &sz]() -> String
    {
        int n = 0;
        ctx._read(&n, sz);
        return strings[n];
    };

    std::map<size_t, std::shared_ptr<builder::Command>> commands;

    auto add_command = [&swctx, &commands, &s, &read_string](size_t id, uint8_t type)
    {
        auto it = commands.find(id);
        if (it == commands.end())
        {
            std::shared_ptr<builder::Command> c;
            switch (type)
            {
            case 1:
            {
                auto c2 = std::make_shared<driver::VSCommand>(swctx);
                //c2->file.fs = s.fs;
                c = c2;
                //c2->file.file = read_string();
            }
                break;
            case 2:
            {
                auto c2 = std::make_shared<driver::GNUCommand>(swctx);
                //c2->file.fs = s.fs;
                c = c2;
                //c2->file.file = read_string();
                c2->deps_file = read_string();
            }
                break;
            case 3:
            {
                auto c2 = std::make_shared<driver::ExecuteBuiltinCommand>(swctx);
                c = c2;
            }
                break;
            default:
                c = std::make_shared<builder::Command>(swctx);
                break;
            }
            commands[id] = c;
            c->fs = s.fs;
            return c;
        }
        return it->second;
    };

    std::unordered_map<builder::Command *, std::vector<size_t>> deps;
    while (!ctx.eof())
    {
        size_t id;
        ctx.read(id);

        uint8_t type = 0;
        ctx.read(type);

        auto c = add_command(id, type);

        c->name = read_string();

        c->program = read_string();
        c->working_directory = read_string();

        size_t n;
        ctx.read(n);
        while (n--)
            c->args.push_back(read_string());

        c->redirectStdin(read_string());
        c->redirectStdout(read_string());
        c->redirectStderr(read_string());

        ctx.read(n);
        while (n--)
        {
            auto k = read_string();
            c->environment[k] = read_string();
        }

        ctx.read(n);
        while (n--)
        {
            ctx.read(id);
            deps[c.get()].push_back(id);
        }

        ctx.read(n);
        while (n--)
            c->addInput(read_string());

        ctx.read(n);
        while (n--)
            c->addIntermediate(read_string());

        ctx.read(n);
        while (n--)
            c->addOutput(read_string());
    }

    for (auto &[c, dep] : deps)
    {
        for (auto &d : dep)
            c->dependencies.insert(commands[d]);
    }

    Commands commands2;
    for (auto &[_, c] : commands)
        commands2.insert(c);
    return Solution::CommandExecutionPlan::createExecutionPlan(commands2);
}

void save(const path &fn, const Solution::CommandExecutionPlan &p)
{
    primitives::BinaryStream ctx;

    auto strings = p.gatherStrings();

    size_t sz;
    if (strings.size() & 0xff000000)
        sz = 4;
    else if (strings.size() & 0xff0000)
        sz = 3;
    else if (strings.size() & 0xff00)
        sz = 2;
    else if (strings.size() & 0xff)
        sz = 1;

    ctx.write(sz);

    ctx.write(strings.size());
    std::map<int, String> strings2;
    for (auto &[s, n] : strings)
        strings2[n] = s;
    for (auto &[_, s] : strings2)
        ctx.write(s);

    auto print_string = [&strings, &ctx, &sz](const String &in)
    {
        auto n = strings[in];
        ctx._write(&n, sz);
    };

    for (auto &c : p.commands)
    {
        ctx.write(c);

        uint8_t type = 0;
        if (auto c2 = c->as<driver::VSCommand>(); c2)
        {
            type = 1;
            ctx.write(type);
            //print_string(c2->file.file.u8string());
        }
        else if (auto c2 = c->as<driver::GNUCommand>(); c2)
        {
            type = 2;
            ctx.write(type);
            //print_string(c2->file.file.u8string());
            print_string(c2->deps_file.u8string());
        }
        else if (auto c2 = c->as<driver::ExecuteBuiltinCommand>(); c2)
        {
            type = 3;
            ctx.write(type);
        }
        else
            ctx.write(type);

        print_string(c->getName());

        print_string(c->program.u8string());
        print_string(c->working_directory.u8string());

        ctx.write(c->args.size());
        for (auto &a : c->args)
            print_string(a);

        print_string(c->in.file.u8string());
        print_string(c->out.file.u8string());
        print_string(c->err.file.u8string());

        ctx.write(c->environment.size());
        for (auto &[k, v] : c->environment)
        {
            print_string(k);
            print_string(v);
        }

        ctx.write(c->dependencies.size());
        for (auto &d : c->dependencies)
            ctx.write(d.get());

        ctx.write(c->inputs.size());
        for (auto &f : c->inputs)
            print_string(f.u8string());

        ctx.write(c->intermediate.size());
        for (auto &f : c->intermediate)
            print_string(f.u8string());

        ctx.write(c->outputs.size());
        for (auto &f : c->outputs)
            print_string(f.u8string());
    }

    fs::create_directories(fn.parent_path());
    ctx.save(fn);
}

void Build::execute()
{
    dry_run = ::dry_run;

    // read ex plan
    if (ide)
    {
        if (remove_ide_explans)
        {
            // remove execution plans
            fs::remove_all(getExecutionPlansDir());
        }

        for (auto &s : solutions)
        {
            auto fn = s.getExecutionPlanFilename();
            if (fs::exists(fn))
            {
                // prevent double assign generators
                fs->reset();

                auto p = ::sw::load(swctx, fn, s);
                s.execute(p);
                return;
            }
        }
    }

    prepare();

    for (auto &[n, _] : TargetsToBuild)
    {
        for (auto &s : solutions)
        {
            auto &t = s.children[n];
            if (!t)
                throw SW_RUNTIME_ERROR("Empty target");
            s.TargetsToBuild[n] = t;
        }
    }

    if (ide)
    {
        // write execution plans
        for (auto &s : solutions)
        {
            auto p = s.getExecutionPlan();
            auto fn = s.getExecutionPlanFilename();
            if (!fs::exists(fn))
                save(fn, p);
        }
    }

    if (getGenerator())
    {
        generateBuildSystem();
        return;
    }

    Solution::execute();

    if (with_testing)
    {
        Commands cmds;
        for (auto &s : solutions)
            cmds.insert(s.tests.begin(), s.tests.end());
        auto p = Solution::getExecutionPlan(cmds);
        Solution::execute(p);
    }
}

void Build::load_configless(const path &file_or_dir)
{
    setupSolutionName(file_or_dir);

    load_dll({}, false);

    bool dir = fs::is_directory(config_file_or_dir);

    Strings comments;
    if (!dir)
    {
        // for generators
        config = file_or_dir;

        auto f = read_file(file_or_dir);

        auto b = f.find("/*");
        if (b != f.npos)
        {
            auto e = f.find("*/", b);
            if (e != f.npos)
            {
                auto s = f.substr(b + 2, e - b - 2);
                if (!s.empty())
                    comments.push_back(s);
            }
        }
    }

    createSolutions("", false);
    for (auto &s : solutions)
    {
        current_solution = &s;
        if (!dir)
        {
            //exe += file_or_dir;

            for (auto &c : comments)
            {
                auto root = YAML::Load(c);
                cppan_load(root, file_or_dir.stem().u8string());
            }

            if (s.children.size() == 1)
            {
                if (auto nt = s.children.begin()->second->as<NativeExecutedTarget>())
                    *nt += file_or_dir;
            }

            TargetsToBuild = s.children;
        }
        else
        {
            auto &exe = s.addExecutable(ide_solution_name);
            bool read_deps_from_comments = false;

            if (!read_deps_from_comments)
            {
                SW_UNIMPLEMENTED; // and never was

                /*for (auto &[p, d] : getPackageStore().resolved_packages)
                {
                    if (d.installed)
                        exe += std::make_shared<Dependency>(p.toString());
                }*/
            }
        }
    }
}

void Build::generateBuildSystem()
{
    if (!getGenerator())
        return;

    getCommands();
    getExecutionPlan(); // also prepare commands

    for (auto &s : solutions)
        fs::remove_all(s.getExecutionPlansDir());
    getGenerator()->generate(*this);
}

static const auto ide_fs = "ide_vs";

// on fast path we do not create a lot of threads in main()
// we do it here
static std::unique_ptr<Executor> e;
static bool fast_path_exit;

void Build::load_packages(const StringSet &pkgs)
{
    if (pkgs.empty())
        return;

    if (!gIdeFastPath.empty())
    {
        if (fs::exists(gIdeFastPath))
        {
            auto files = read_lines(gIdeFastPath);
            if (std::none_of(files.begin(), files.end(), [this](auto &f) {
                return File(f, swctx.getFileStorage(ide_fs, true)).isChanged();
                }))
            {
                fast_path_exit = true;
                return;
            }
            solutions.clear();
        }
        e = std::make_unique<Executor>(select_number_of_threads(gNumberOfJobs));
        getExecutor(e.get());
    }

    //
    UnresolvedPackages upkgs;
    for (auto &p : pkgs)
        upkgs.insert(p);

    // resolve only deps needed
    auto m = swctx.install(upkgs);

    for (auto &[u, p] : m)
        knownTargets.insert(p);

    std::unordered_map<PackageVersionGroupNumber, LocalPackage> cfgs2;
    for (auto &[u, p] : m)
    {
        knownTargets.insert(p);
        // gather packages
        cfgs2.emplace(p.getData().group_number, p);
    }
    std::unordered_set<LocalPackage> cfgs;
    for (auto &[gn, s] : cfgs2)
        cfgs.insert(s);

    Local = false;
    configure = false;

    auto dll = ::sw::build_configs(swctx, cfgs);

    SwapAndRestore sr(NamePrefix, cfgs.begin()->ppath.slice(0, cfgs.begin()->getData().prefix));
    if (cfgs.size() != 1)
        sr.restoreNow(true);

    createSolutions(dll, true);
    // set known targets to allow target loading
    for (auto &s : solutions)
        s.knownTargets = knownTargets;
    load_dll(dll);

    // clear TargetsToBuild that is set inside load_dll()
    for (auto &s : solutions)
        s.TargetsToBuild.clear();

    // now we set ours TargetsToBuild to this object
    // execute() will propagate them to solutions
    for (auto &[porig, p] : m)
        TargetsToBuild[p];
}

void Build::build_packages(const StringSet &pkgs)
{
    if (pkgs.empty())
        return;

    load_packages(pkgs);
    if (fast_path_exit)
        return;
    execute();

    //
    if (!gIdeFastPath.empty())
    {
        UnresolvedPackages upkgs;
        for (auto &p : pkgs)
            upkgs.insert(p);
        auto pkgs2 = swctx.resolve(upkgs);

        // at the moment we have one solution here
        Files files;
        Commands cmds;
        for (auto &[u, p] : pkgs2)
        {
            auto i = solutions[0].children.find(p);
            if (i == solutions[0].children.end())
                throw SW_RUNTIME_ERROR("No such target in fast path: " + p.toString());
            if (auto nt = i->second->as<NativeExecutedTarget>())
            {
                if (auto c = nt->getCommand())
                {
                    files.insert(c->outputs.begin(), c->outputs.end());

                    if (*nt->HeaderOnly)
                        continue;
                    if (nt->getSelectedTool() == nt->Librarian.get())
                        continue;
                    if (isExecutable(nt->getType()))
                        continue;

                    if (nt->Scope == TargetScope::Build)
                    {
                        auto dt = nt;
                        if (getSolution()->Settings.Native.LibrariesType != LibraryType::Shared && !dt->isSharedOnly())
                            continue;
                        auto in = dt->getOutputFile();
                        auto o = gIdeCopyToDir / dt->NativeTarget::getOutputDir();
                        o /= in.filename();
                        if (in == o)
                            continue;

                        SW_MAKE_EXECUTE_BUILTIN_COMMAND(copy_cmd, *nt, "sw_copy_file");
                        copy_cmd->args.push_back(in.u8string());
                        copy_cmd->args.push_back(o.u8string());
                        copy_cmd->addInput(dt->getOutputFile());
                        copy_cmd->addOutput(o);
                        //copy_cmd->dependencies.insert(nt->getCommand());
                        copy_cmd->name = "copy: " + normalize_path(o);
                        copy_cmd->maybe_unused = builder::Command::MU_ALWAYS;
                        copy_cmd->command_storage = builder::Command::CS_LOCAL;
                        cmds.insert(copy_cmd);

                        files.insert(o);
                    }
                }
            }
        }

        // perform copy
        solutions[0].getExecutionPlan(cmds).execute(getExecutor());

        String s;
        for (auto &f : files)
        {
            s += normalize_path(f) + "\n";
            File(f, swctx.getFileStorage(ide_fs, true)).isChanged();
        }
        write_file(gIdeFastPath, s);
    }
}

void Build::run_package(const String &s)
{
    build_packages({ s });

    auto nt = solutions[0].getTargetPtr(swctx.resolve(extractFromString(s)))->as<NativeExecutedTarget>();
    if (!nt || nt->getType() != TargetType::NativeExecutable)
        throw SW_RUNTIME_ERROR("Unsupported package type");

    auto cb = nt->addCommand();

    cb.c->always = true;
    cb.c->program = nt->getOutputFile();
    cb.c->working_directory = nt->getPackage().getDirObjWdir();
    fs::create_directories(cb.c->working_directory);
    nt->setupCommandForRun(*cb.c);
    //if (cb.c->create_new_console)
    //{
        //cb.c->inherit = true;
        //cb.c->in.inherit = true;
    //}
    //else
        cb.c->detached = true;

    run(nt->getPackage(), *cb.c);
}

static bool hasAnyUserProvidedInformation()
{
    return 0
        || !configuration.empty()
        || static_build
        || shared_build
        || win_mt
        || win_md
        || !platform.empty()
        || !compiler.empty()
        || !target_os.empty()
        || !libc.empty()
        ;

    //|| (static_build && shared_build) // when both; but maybe ignore?
    //|| (win_mt && win_md) // when both; but maybe ignore?

}

static bool hasUserProvidedInformationStrong()
{
    return 0
        || !configuration.empty()
        || !compiler.empty()
        || !target_os.empty()
        ;
}

void Build::createSolutions(const path &dll, bool usedll)
{
    if (gWithTesting)
        with_testing = true;

    if (solutions_created)
        return;
    solutions_created = true;

    if (usedll)
        sw_check_abi_version(getModuleStorage(base_ptr).get(dll).sw_get_module_abi_version());

    // configure may change defaults, so we must care below
    if (usedll && configure)
        getModuleStorage(base_ptr).get(dll).configure(*this);

    if (hasAnyUserProvidedInformation())
    {
        if (append_configs || !hasUserProvidedInformationStrong())
        {
            if (auto g = getGenerator())
            {
                g->createSolutions(*this);
            }
        }

        // one more time, if generator did not add solution or whatever
        addFirstSolution();

        auto times = [this](int n)
        {
            if (n <= 1)
                return;
            auto s2 = solutions;
            for (int i = 1; i < n; i++)
            {
                for (auto &s : s2)
                    solutions.push_back(s);
            }
        };

        auto mult_and_action = [this, &times](int n, auto f)
        {
            times(n);
            for (int i = 0; i < n; i++)
            {
                int mult = solutions.size() / n;
                for (int j = i * mult; j < (i + 1) * mult; j++)
                    f(solutions[j], i);
            }
        };

        // configuration
        auto set_conf = [this](auto &s, const String &configuration)
        {
            auto t = configurationTypeFromStringCaseI(configuration);
            if (toIndex(t))
                s.Settings.Native.ConfigurationType = t;
        };

        Strings configs;
        for (auto &c : configuration)
        {
            if (used_configs.find(c) == used_configs.end())
            {
                if (isConfigSelected(c))
                    LOG_WARN(logger, "config was not used: " + c);
            }
            if (!isConfigSelected(c))
                configs.push_back(c);
        }
        mult_and_action(configs.size(), [&set_conf, &configs](auto &s, int i)
        {
            set_conf(s, configs[i]);
        });

        // static/shared
        if (static_build && shared_build)
        {
            mult_and_action(2, [&set_conf](auto &s, int i)
            {
                if (i == 0)
                    s.Settings.Native.LibrariesType = LibraryType::Static;
                if (i == 1)
                    s.Settings.Native.LibrariesType = LibraryType::Shared;
            });
        }
        else
        {
            for (auto &s : solutions)
            {
                if (static_build)
                    s.Settings.Native.LibrariesType = LibraryType::Static;
                if (shared_build)
                    s.Settings.Native.LibrariesType = LibraryType::Shared;
            }
        }

        // mt/md
        if (win_mt && win_md)
        {
            mult_and_action(2, [&set_conf](auto &s, int i)
            {
                if (i == 0)
                    s.Settings.Native.MT = true;
                if (i == 1)
                    s.Settings.Native.MT = false;
            });
        }
        else
        {
            for (auto &s : solutions)
            {
                if (win_mt)
                    s.Settings.Native.MT = true;
                if (win_md)
                    s.Settings.Native.MT = false;
            }
        }

        // platform
        auto set_pl = [](auto &s, const String &platform)
        {
            auto t = archTypeFromStringCaseI(platform);
            if (toIndex(t))
                s.Settings.TargetOS.Arch = t;
        };

        mult_and_action(platform.size(), [&set_pl](auto &s, int i)
        {
            set_pl(s, platform[i]);
        });

        // compiler
        auto set_cl = [](auto &s, const String &compiler)
        {
            auto t = compilerTypeFromStringCaseI(compiler);
            if (toIndex(t))
                s.Settings.Native.CompilerType = t;
        };

        mult_and_action(compiler.size(), [&set_cl](auto &s, int i)
        {
            set_cl(s, compiler[i]);
        });

        // target_os
        auto set_tos = [](auto &s, const String &target_os)
        {
            auto t = OSTypeFromStringCaseI(target_os);
            if (toIndex(t))
                s.Settings.TargetOS.Type = t;
        };

        mult_and_action(target_os.size(), [&set_tos](auto &s, int i)
        {
            set_tos(s, target_os[i]);
        });

        // libc
        /*auto set_libc = [](auto &s, const String &libc)
        {
            s.Settings.Native.libc = libc;
        };

        mult_and_action(libc.size(), [&set_libc](auto &s, int i)
        {
            set_libc(s, libc[i]);
        });*/
    }
    else if (auto g = getGenerator())
    {
        g->createSolutions(*this);
    }

    // one more time, if generator did not add solution or whatever
    addFirstSolution();
}

void Build::load_dll(const path &dll, bool usedll)
{
    createSolutions(dll, usedll);

    // add cc if needed
    getHostSolution();

    for (auto &s : solutions)
    {
        // apply config settings
        s.findCompiler();

        // initiate libc
        /*if (s.Settings.Native.libc)
        {
            Resolver r;
            r.resolve_dependencies({ *s.Settings.Native.libc });
            auto dd = r.getDownloadDependencies();
            for (auto &p : dd)
                s.knownTargets.insert(p);

            // gather packages
            std::unordered_set<ExtendedPackageData> cfgs;
            for (auto &[p, _] : r.getDownloadDependenciesWithGroupNumbers())
                cfgs.insert(p);

            auto dll = ::sw::build_configs(cfgs);
            auto b = *this;
            b.solutions[i].Settings.Native.libc.reset();
            b.load_dll(dll);

            // copy back prepared programs (compilers, linkers etc.)
            (LanguageStorage&)s = (LanguageStorage&)b.solutions[i];
        }*/
    }

    // detect and eliminate solution clones?

    if (auto g = getGenerator())
    {
        g->initSolutions(*this);
    }

    // print info
    if (auto g = getGenerator())
    {
        LOG_INFO(logger, "Generating " << toString(g->type) << " project with " << solutions.size() << " configurations:");
        for (auto &s : solutions)
            LOG_INFO(logger, s.getConfig());
    }
    else
    {
        LOG_DEBUG(logger, (getGenerator() ? "Generating " + toString(getGenerator()->type) + " " : "Building ")
            << "project with " << solutions.size() << " configurations:");
        for (auto &s : solutions)
            LOG_DEBUG(logger, s.getConfig());
    }

    // check
    {
        // some packages want checks in their build body
        // because they use variables from checks

        // make parallel?
        if (usedll)
        {
            for (auto &s : solutions)
                getModuleStorage(base_ptr).get(dll).check(s, s.checker);
        }
        performChecks();
    }

    // build
    if (usedll)
    {
        for (const auto &[i,s] : enumerate(solutions))
        {
            if (solutions.size() > 1)
                LOG_INFO(logger, "[" << (i + 1) << "/" << solutions.size() << "] load pass " << s.getConfig());
            getModuleStorage(base_ptr).get(dll).build(s);
        }
    }

    // we build only targets from this package
    // for example, on linux we do not build skipped windows projects
    for (auto &s : solutions)
    {
        // only exception is cc host solution
        if (getHostSolution() == &s)
            continue;
        s.TargetsToBuild = s.children;
    }
}

const Solution *Build::getHostSolution() const
{
    if (host)
        return host.value();
    throw SW_RUNTIME_ERROR("no host solution selected");
}

const Solution *Build::getHostSolution()
{
    if (host)
        return host.value();

    auto needs_cc = [](auto &s)
    {
        return !s.HostOS.canRunTargetExecutables(s.Settings.TargetOS);
    };

    if (std::any_of(solutions.begin(), solutions.end(), needs_cc))
    {
        LOG_DEBUG(logger, "Cross compilation is required");
        for (auto &s : solutions)
        {
            if (!needs_cc(s))
            {
                LOG_DEBUG(logger, "CC solution was found");
                host = &s;
                break;
            }
        }
        if (!host)
        {
            // add
            LOG_DEBUG(logger, "Cross compilation solution was not found, creating a new one");
            auto &s = addSolution();
            host = &s;
        }
    }
    else
        host = nullptr;

    return host.value();
}

bool Build::isConfigSelected(const String &s) const
{
    try
    {
        configurationTypeFromStringCaseI(s);
        return false; // conf is known and reserved!
    }
    catch (...) {}

    used_configs.insert(s);

    static const StringSet cfgs(configuration.begin(), configuration.end());
    return cfgs.find(s) != cfgs.end();
}

}
