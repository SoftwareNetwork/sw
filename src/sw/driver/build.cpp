// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "build.h"

#include "driver.h"
#include "frontend/cppan/yaml.h"
#include "functions.h"
#include "inserts.h"
#include "module.h"
#include "run.h"
#include "suffix.h"
#include "sw_abi_version.h"
#include "target/native.h"

#include <sw/builder/execution_plan.h>
#include <sw/builder/file_storage.h>
#include <sw/builder/program.h>
#include <sw/core/sw_context.h>
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

static cl::opt<bool> debug_configs("debug-configs", cl::desc("Build configs in debug mode"));

bool gVerbose;
bool gWithTesting;
path gIdeFastPath;
path gIdeCopyToDir;
int gNumberOfJobs = -1;

std::map<sw::PackagePath, sw::Version> gUserSelectedPackages;

// TODO: add '#pragma sw driver ...'

// Note
// We have separate Build for configs, because stored build scripts
// may differ from remote ones (until we deny overriding package versions).
// Later we may have these rules stored in memory in order to load and build subsequents requests.
// But we also have overridden packages which breaks everything above.

namespace sw
{

NativeTargetEntryPoint::NativeTargetEntryPoint(Build &b)
    : b(b)
{
}

void NativeTargetEntryPoint::loadPackages(const TargetSettings &s, const PackageIdSet &pkgs)
{
    module_data.ntep = shared_from_this();
    SwapAndRestore sr1(module_data.known_targets, pkgs);
    if (pkgs.empty())
        sr1.restoreNow(true);
    SwapAndRestore sr2(b.module_data, &module_data);
    SwapAndRestore sr3(b.NamePrefix, module_data.NamePrefix);
    SwapAndRestore sr4(module_data.current_settings, s);
    SwapAndRestore sr5(module_data.bs, BuildSettings(s));
    loadPackages1();
}

void NativeTargetEntryPoint::addChild(const TargetBaseTypePtr &t)
{
    if (t->DryRun)
        b.getChildren()[t->getPackage()].push_back_inactive(t);
    else
        b.getChildren()[t->getPackage()].push_back(t);
    b.getChildren()[t->getPackage()].setEntryPoint(shared_from_this());
    module_data.added_targets.push_back(t.get());
}

NativeBuiltinTargetEntryPoint::NativeBuiltinTargetEntryPoint(Build &b, BuildFunction bf/*, const BuildSettings &s*/)
    : NativeTargetEntryPoint(b), bf(bf)
{
}

void NativeBuiltinTargetEntryPoint::loadPackages1()
{
    if (cf)
        cf(b.checker);
    if (!bf)
        throw SW_RUNTIME_ERROR("No internal build function set");
    bf(b);
}

struct SW_DRIVER_CPP_API NativeModuleTargetEntryPoint : NativeTargetEntryPoint
{
    NativeModuleTargetEntryPoint(Build &b, const Module &m)
        : NativeTargetEntryPoint(b), m(m)
    {
    }

private:
    Module m;

    void loadPackages1() override
    {
        m.check(b, b.checker);
        m.build(b);
    }
};

namespace detail
{

void EventCallback::operator()(Target &t, CallbackType e)
{
    if (!pkgs.empty() && pkgs.find(t.getPackage()) == pkgs.end())
        return;
    if (!types.empty() && types.find(e) == types.end())
        return;
    if (types.empty() && typed_cb)
        throw std::logic_error("Typed callback passed, but no types provided");
    if (!cb)
        throw std::logic_error("No callback provided");
    cb(t, e);
}

}

String toString(FrontendType t)
{
    switch (t)
    {
    case FrontendType::Sw:
        return "sw";
    case FrontendType::Cppan:
        return "cppan";
    default:
        throw std::logic_error("not implemented");
    }
}

static String getCurrentModuleId()
{
    return shorten_hash(sha1(getProgramName()), 6);
}

static path getImportFilePrefix(const SwBuilderContext &swctx)
{
    static const String pch_ver = "1";
    return swctx.getLocalStorage().storage_dir_tmp / ("sw." + pch_ver + "." + getCurrentModuleId());
}

static path getImportDefinitionsFile(const SwBuilderContext &swctx)
{
    return getImportFilePrefix(swctx) += ".def";
}

static path getImportLibraryFile(const SwBuilderContext &swctx)
{
    return getImportFilePrefix(swctx) += ".lib";
}

static path getImportPchFile(const SwBuilderContext &swctx)
{
    return getImportFilePrefix(swctx) += ".cpp";
}

#ifdef _WIN32
static Strings getExports(HMODULE lib)
{
    auto header = (PIMAGE_NT_HEADERS)((BYTE *)lib + ((PIMAGE_DOS_HEADER)lib)->e_lfanew);
    auto exports = (PIMAGE_EXPORT_DIRECTORY)((BYTE *)lib + header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
    auto names = (int*)((uint64_t)lib + exports->AddressOfNames);
    Strings syms;
    for (DWORD i = 0; i < exports->NumberOfNames; i++)
    {
        const char *n = (const char *)lib + names[i];
        syms.push_back(n);
    }
    return syms;
}
#endif

static void addImportLibrary(const SwBuilderContext &swctx, NativeCompiledTarget &t)
{
#ifdef _WIN32
    auto lib = (HMODULE)primitives::getModuleForSymbol();
    auto syms = getExports(lib);
    if (syms.empty())
        throw SW_RUNTIME_ERROR("No exports found");
    String defs;
    defs += "LIBRARY " IMPORT_LIBRARY "\n";
    defs += "EXPORTS\n";
    for (auto &s : syms)
        defs += "    "s + s + "\n";
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
    auto prefix = f.substr(0, pos);
    auto nlines = std::count(prefix.begin(), prefix.end(), '\n') + 2;
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
        ctx.addLine("#line " + std::to_string(nlines) + " \"" + normalize_path(cfg) + "\"");

        primitives::Emitter prefix;
        auto ins_pre = "#pragma sw header insert prefix";
        if (f.find(ins_pre) != f.npos)
            boost::replace_all(f, ins_pre, prefix.getText());
        else
            ctx += prefix;

        ctx.addLine(f);
        ctx.addLine();

        write_file_if_different(h, ctx.getText());
    }
    return h;
}

static std::tuple<FilesOrdered, UnresolvedPackages> getFileDependencies(const SwBuilderContext &swctx, const path &p, std::set<PackageVersionGroupNumber> &gns)
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
            if (!gns.insert(pkg.getData().group_number).second)
                throw SW_RUNTIME_ERROR("#pragma sw header: trying to add same header twice, last one: " + upkg.toString());
            auto h = getPackageHeader(pkg, upkg);
            auto [headers2,udeps2] = getFileDependencies(swctx, h, gns);
            headers.insert(headers.end(), headers2.begin(), headers2.end());
            udeps.insert(udeps2.begin(), udeps2.end());
            headers.push_back(h);
        }
        else if (m1 == "local")
        {
            SW_UNIMPLEMENTED;
            auto [headers2, udeps2] = getFileDependencies(swctx, m[3].str(), gns);
            headers.insert(headers.end(), headers2.begin(), headers2.end());
            udeps.insert(udeps2.begin(), udeps2.end());
        }
        else
            udeps.insert(extractFromString(m1));
        f = m.suffix().str();
}

    return { headers, udeps };
}

static std::tuple<FilesOrdered, UnresolvedPackages> getFileDependencies(const SwBuilderContext &swctx, const path &in_config_file)
{
    std::set<PackageVersionGroupNumber> gns;
    return getFileDependencies(swctx, in_config_file, gns);
}

static void sw_check_abi_version(int v)
{
    if (v > SW_MODULE_ABI_VERSION)
        throw SW_RUNTIME_ERROR("Module ABI (" + std::to_string(v) + ") is greater than binary ABI (" + std::to_string(SW_MODULE_ABI_VERSION) + "). Update your sw binary.");
    if (v < SW_MODULE_ABI_VERSION)
        throw SW_RUNTIME_ERROR("Module ABI (" + std::to_string(v) + ") is less than binary ABI (" + std::to_string(SW_MODULE_ABI_VERSION) + "). Update sw driver headers (or ask driver maintainer).");
}

static String gn2suffix(PackageVersionGroupNumber gn)
{
    return "_" + (gn > 0 ? std::to_string(gn) : ("_" + std::to_string(-gn)));
}

Build::Build(const SwContext &swctx, SwBuild &mb, const driver::cpp::Driver &driver)
    : swctx(swctx)
    , main_build(mb)
    , driver(driver)
    , checker(*this)
{
    host_settings = swctx.getHostSettings();

    // canonical makes disk letter uppercase on windows
    setSourceDirectory(swctx.source_dir);
    BinaryDir = SourceDir / SW_BINARY_DIR;
}

Build::Build(const Build &rhs)
    : Base(rhs)
    , swctx(rhs.swctx)
    , main_build(rhs.main_build)
    , driver(rhs.driver)
    , source_dirs_by_source(rhs.source_dirs_by_source)
    , command_storage(rhs.command_storage)
    , checker(*this)
    , host_settings(rhs.host_settings)
{
}

Build::~Build()
{
}

const OS &Build::getHostOs() const
{
    return swctx.HostOS;
}

path Build::getChecksDir() const
{
    return getServiceDir() / "checks";
}

static auto getFilesHash(const Files &files)
{
    String h;
    for (auto &fn : files)
        h += fn.u8string();
    return shorten_hash(blake2b_512(h), 6);
}

static PackagePath getSelfTargetName(const Files &files)
{
    return "loc.sw.self." + getFilesHash(files);
}

#define SW_DRIVER_NAME "org.sw.sw.client.driver.cpp-0.3.1"
#define SW_DRIVER_INCLUDE_DIR "src"

static NativeCompiledTarget &getDriverTarget(Build &solution, NativeCompiledTarget &lib)
{
    auto i = solution.getChildren().find(UnresolvedPackage(SW_DRIVER_NAME));
    if (i == solution.getChildren().end())
        throw SW_RUNTIME_ERROR("no driver target");
    auto k = i->second.find(lib.getTargetSettings());
    if (k == i->second.end())
    {
        i->second.loadPackages(solution.getSettings(), solution.module_data->known_targets);
        k = i->second.find(lib.getTargetSettings());
        if (k == i->second.end())
        {
            //if (i->second.empty())
                throw SW_RUNTIME_ERROR("no driver target (by settings)");
            //k = i->second.begin();
        }
    }
    return (*k)->as<NativeCompiledTarget>();
}

static void addDeps(Build &solution, NativeCompiledTarget &lib)
{
    lib += "pub.egorpugin.primitives.templates-master"_dep; // for SW_RUNTIME_ERROR

    // uncomment when you need help
    //lib += "pub.egorpugin.primitives.source-master"_dep;
    //lib += "pub.egorpugin.primitives.version-master"_dep;
    //lib += "pub.egorpugin.primitives.command-master"_dep;
    //lib += "pub.egorpugin.primitives.filesystem-master"_dep;

    auto &drv = getDriverTarget(solution, lib);
    auto d = lib + drv;
    d->IncludeDirectoriesOnly = true;

    // generated file, because it won't build in IncludeDirectoriesOnly
    // FIXME: ^^^ this is wrong, generated header must be built in idirs only
    lib += drv.BinaryDir / "options_cl.generated.h";
}

// add Dirs?
static path getDriverIncludeDir(Build &solution, NativeCompiledTarget &lib)
{
    return getDriverTarget(solution, lib).SourceDir / SW_DRIVER_INCLUDE_DIR;
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

static void write_pch(Build &solution)
{
    write_file_if_different(getImportPchFile(solution.swctx),
        //"#include <" + normalize_path(getDriverIncludeDir(solution) / getMainPchFilename()) + ">\n\n" +
        //"#include <" + getDriverIncludePathString(solution, getMainPchFilename()) + ">\n\n" +
        //"#include <" + normalize_path(getMainPchFilename()) + ">\n\n" + // the last one
        cppan_cpp);
}

const ModuleSwappableData &Build::getModuleData() const
{
    if (!module_data)
        throw SW_LOGIC_ERROR("no module data was set");
    return *module_data;
}

const TargetSettings &Build::getSettings() const
{
    return getModuleData().current_settings;
}

const BuildSettings &Build::getBuildSettings() const
{
    return getModuleData().bs;
}

const TargetSettings &Build::getHostSettings() const
{
    return host_settings;
}

PackageVersionGroupNumber Build::getCurrentGroupNumber() const
{
    return getModuleData().current_gn;
}

const String &Build::getCurrentModule() const
{
    return getModuleData().current_module;
}

void Build::addChild(const TargetBaseTypePtr &t)
{
    auto p = getModuleData().ntep.lock();
    if (!p)
        throw SW_RUNTIME_ERROR("Entry point was not set");
    p->addChild(t);
}

struct PrepareConfigEntryPoint : NativeTargetEntryPoint
{
    using NativeTargetEntryPoint::NativeTargetEntryPoint;

    path out;
    FilesMap r;
    std::unique_ptr<PackageId> tgt;

    PrepareConfigEntryPoint(Build &b, const std::unordered_set<LocalPackage> &pkgs)
        : NativeTargetEntryPoint(b), pkgs_(pkgs)
    {}

    PrepareConfigEntryPoint(Build &b, const Files &files)
        : NativeTargetEntryPoint(b), files_(files)
    {}

private:
    const std::unordered_set<LocalPackage> pkgs_;
    Files files_;

    void loadPackages1() override
    {
        b.build_self();

        if (files_.empty())
            many2one(pkgs_);
        else
            many2many(files_);
    }

    SharedLibraryTarget &createTarget(const Files &files)
    {
        b.IsConfig = true;
        auto &lib = b.addTarget<SharedLibraryTarget>(getSelfTargetName(files), "local");
        tgt = std::make_unique<PackageId>(lib.getPackage());
        b.IsConfig = false;
        return lib;
    }

    decltype(auto) commonActions(const Files &files)
    {
        auto &lib = createTarget(files);

        addDeps(b, lib);
        addImportLibrary(b.swctx, lib);
        lib.AutoDetectOptions = false;
        lib.CPPVersion = CPPLanguageStandard::CPP17;

        // add files
        for (auto &fn : files)
            lib += fn;

        //
        write_pch(b);
        PrecompiledHeader pch;
        pch.header = getDriverIncludeDir(b, lib) / getMainPchFilename();
        pch.source = getImportPchFile(b.swctx);
        pch.force_include_pch = true;
        pch.force_include_pch_to_source = true;
        lib.addPrecompiledHeader(pch);

        return lib;
    }

    void commonActions2(SharedLibraryTarget &lib)
    {
        lib += "SW_CPP_DRIVER_API_VERSION=1"_def;

        if (lib.getSettings().TargetOS.is(OSType::Windows))
        {
            lib.Definitions["SW_SUPPORT_API"] = "__declspec(dllimport)";
            lib.Definitions["SW_MANAGER_API"] = "__declspec(dllimport)";
            lib.Definitions["SW_BUILDER_API"] = "__declspec(dllimport)";
            lib.Definitions["SW_DRIVER_CPP_API"] = "__declspec(dllimport)";
            // do not use api name because we use C linkage
            lib.Definitions["SW_PACKAGE_API"] = "__declspec(dllexport)";
        }
        else
        {
            lib.Definitions["SW_SUPPORT_API="];
            lib.Definitions["SW_MANAGER_API="];
            lib.Definitions["SW_BUILDER_API="];
            lib.Definitions["SW_DRIVER_CPP_API="];
            // do not use api name because we use C linkage
            lib.Definitions["SW_PACKAGE_API"] = "__attribute__ ((visibility (\"default\")))";
        }

        BuildSettings bs(module_data.current_settings);
        if (bs.TargetOS.is(OSType::Windows))
            lib.NativeLinkerOptions::System.LinkLibraries.insert("Delayimp.lib");

        if (auto L = lib.Linker->template as<VisualStudioLinker*>())
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

        auto i = b.getChildren().find(lib.getPackage());
        if (i == b.getChildren().end())
            throw std::logic_error("config target not found");
        b.TargetsToBuild[i->first] = i->second;

        out = lib.getOutputFile();
    }

    // many input files to many dlls
    void many2many(const Files &files)
    {
        for (auto &fn : files)
        {
            one2one(fn);
            r[fn] = out;
        }
    }

    // many input files into one dll
    void many2one(const std::unordered_set<LocalPackage> &pkgs)
    {
        // make parallel?
        auto get_package_config = [](const auto &pkg) -> path
        {
            if (pkg.getData().group_number)
            {
                auto d = findConfig(pkg.getDirSrc2(), Build::getAvailableFrontendConfigFilenames());
                if (!d)
                    throw SW_RUNTIME_ERROR("cannot find config");
                return *d;
            }
            auto p = pkg.getGroupLeader();
            if (auto d = findConfig(p.getDirSrc2(), Build::getAvailableFrontendConfigFilenames()))
                return *d;
            auto d = findConfig(pkg.getDirSrc2(), Build::getAvailableFrontendConfigFilenames());
            if (!d)
                throw SW_RUNTIME_ERROR("cannot find config");
            fs::create_directories(p.getDirSrc2());
            fs::copy_file(*d, p.getDirSrc2() / d->filename());
            return p.getDirSrc2() / d->filename();
        };

        Files files;
        std::unordered_map<path, LocalPackage> output_names;
        for (auto &pkg : pkgs)
        {
            auto p = get_package_config(pkg);
            files.insert(p);
            output_names.emplace(p, pkg);
        }

        auto &lib = commonActions(files);

        // make fancy names
        for (auto &[fn, pkg] : output_names)
        {
            lib[fn].fancy_name = "[" + output_names.find(fn)->second.toString() + "]/[config]";
            // configs depend on pch, and pch depends on getCurrentModuleId(), so we add name to the file
            // to make sure we have different config .objs for different pchs
            lib[fn].as<NativeSourceFile>().setOutputFile(lib, fn.u8string() + "." + getCurrentModuleId(), lib.getObjectDir(pkg) / "self");
            if (gVerbose)
                lib[fn].fancy_name += " (" + normalize_path(fn) + ")";
        }

        // file deps
        auto gnu_setup = [this, &lib](auto *c, const auto &headers, const path &fn, LocalPackage &pkg)
        {
            // we use pch, but cannot add more defs on CL
            // so we create a file with them
            auto gn = pkg.getData().group_number;
            auto hash = gn2suffix(gn);
            path h;
            // cannot create aux dir on windows; auxl = auxiliary
            if (is_under_root(fn, b.swctx.getLocalStorage().storage_dir_pkg))
                h = fn.parent_path().parent_path() / "auxl" / ("defs" + hash + ".h");
            else
                h = fn.parent_path() / SW_BINARY_DIR / "auxl" / ("defs" + hash + ".h");
            primitives::CppEmitter ctx;

            ctx.addLine("#define configure configure" + hash);
            ctx.addLine("#define build build" + hash);
            ctx.addLine("#define check check" + hash);
            ctx.addLine("#define sw_get_module_abi_version sw_get_module_abi_version" + hash);

            write_file_if_different(h, ctx.getText());

            c->ForcedIncludeFiles().push_back(h);
            c->ForcedIncludeFiles().push_back(getDriverIncludeDir(b, lib) / getSw1Header());

            for (auto &h : headers)
                c->ForcedIncludeFiles().push_back(h);
            c->ForcedIncludeFiles().push_back(getDriverIncludeDir(b, lib) / getSwCheckAbiVersionHeader());
        };

        for (auto &[fn, pkg] : output_names)
        {
            auto [headers, udeps] = getFileDependencies(b.swctx, fn);
            if (auto sf = lib[fn].template as<NativeSourceFile*>())
            {
                if (auto c = sf->compiler->template as<VisualStudioCompiler*>())
                {
                    gnu_setup(c, headers, fn, pkg);
                }
                else if (auto c = sf->compiler->template as<ClangClCompiler*>())
                {
                    gnu_setup(c, headers, fn, pkg);
                }
                else if (auto c = sf->compiler->template as<ClangCompiler*>())
                {
                    gnu_setup(c, headers, fn, pkg);
                }
                else if (auto c = sf->compiler->template as<GNUCompiler*>())
                {
                    gnu_setup(c, headers, fn, pkg);
                }
            }
            for (auto &d : udeps)
                lib += std::make_shared<Dependency>(d);
        }

        commonActions2(lib);
    }

    // one input file to one dll
    void one2one(const path &fn)
    {
        auto &lib = commonActions({ fn });

        // turn on later again
        //if (lib.getSettings().TargetOS.is(OSType::Windows))
            //lib += "_CRT_SECURE_NO_WARNINGS"_def;

        // file deps
        {
            auto [headers, udeps] = getFileDependencies(b.swctx, fn);
            for (auto &h : headers)
            {
                // TODO: refactor this and same cases below
                if (auto sf = lib[fn].template as<NativeSourceFile *>())
                {
                    if (auto c = sf->compiler->template as<VisualStudioCompiler *>())
                    {
                        c->ForcedIncludeFiles().push_back(h);
                    }
                    else if (auto c = sf->compiler->template as<ClangClCompiler *>())
                    {
                        c->ForcedIncludeFiles().push_back(h);
                    }
                    else if (auto c = sf->compiler->template as<ClangCompiler *>())
                    {
                        c->ForcedIncludeFiles().push_back(h);
                    }
                    else if (auto c = sf->compiler->template as<GNUCompiler *>())
                    {
                        c->ForcedIncludeFiles().push_back(h);
                    }
                }
            }
            for (auto &d : udeps)
                lib += std::make_shared<Dependency>(d);
        }

        if (auto sf = lib[fn].template as<NativeSourceFile*>())
        {
            if (auto c = sf->compiler->template as<VisualStudioCompiler*>())
            {
                c->ForcedIncludeFiles().push_back(getDriverIncludeDir(b, lib) / getSw1Header());
                c->ForcedIncludeFiles().push_back(getDriverIncludeDir(b, lib) / getSwCheckAbiVersionHeader());

                // deprecated warning
                // activate later
                // this causes cl warning (PCH is built without it)
                // we must build two PCHs? for storage pks and local pkgs
                //c->Warnings().TreatAsError.push_back(4996);
            }
            else if (auto c = sf->compiler->template as<ClangClCompiler*>())
            {
                c->ForcedIncludeFiles().push_back(getDriverIncludeDir(b, lib) / getSw1Header());
                c->ForcedIncludeFiles().push_back(getDriverIncludeDir(b, lib) / getSwCheckAbiVersionHeader());
            }
            else if (auto c = sf->compiler->template as<ClangCompiler*>())
            {
                //c->ForcedIncludeFiles().push_back(getDriverIncludeDir(solution) / getSw1Header());
                c->ForcedIncludeFiles().push_back(getDriverIncludeDir(b, lib) / getSwCheckAbiVersionHeader());
            }
            else if (auto c = sf->compiler->template as<GNUCompiler*>())
            {
                //c->ForcedIncludeFiles().push_back(getDriverIncludeDir(solution) / getSw1Header());
                c->ForcedIncludeFiles().push_back(getDriverIncludeDir(b, lib) / getSwCheckAbiVersionHeader());
            }
        }

        commonActions2(lib);
    }
};

template <class T>
std::shared_ptr<PrepareConfigEntryPoint> Build::build_configs1(const T &objs)
{
    TargetSettings ts = host_settings;
    ts["native"]["library"] = "static";
    if (debug_configs)
        ts["native"]["configuration"] = "debug";

    auto ep = std::make_shared<PrepareConfigEntryPoint>(*this, objs);
    ep->loadPackages(ts, {}); // load all

    //execute();

    return ep;
}

FilesMap Build::build_configs_separate(const Files &files)
{
    if (files.empty())
        return {};
    return build_configs1(files)->r;
}

path Build::build_configs(const std::unordered_set<LocalPackage> &pkgs)
{
    if (pkgs.empty())
        return {};
    return build_configs1(pkgs)->out;
}

// can be used in configs to load subdir configs
// s.build->loadModule("client/sw.cpp").call<void(Solution &)>("build", s);
Module Build::loadModule(const path &p) const
{
    SW_UNIMPLEMENTED;

    auto fn2 = p;
    if (!fn2.is_absolute())
        fn2 = SourceDir / fn2;

    auto mb2 = swctx.createBuild();
    Build b(main_build.swctx, mb2, driver);
    b.getChildren() = swctx.getPredefinedTargets();
    path dll;
    {
        auto r = b.build_configs_separate({ fn2 });
        dll = r.begin()->second;
    }
    return swctx.getModuleStorage().get(dll);
}

path Build::build(const path &fn)
{
    if (fs::is_directory(fn))
        throw SW_RUNTIME_ERROR("Filename expected");

    auto fe = selectFrontendByFilename(fn);
    if (!fe)
        throw SW_RUNTIME_ERROR("Unknown frontend config: " + fn.u8string());

    switch (fe.value())
    {
    case FrontendType::Sw:
    {
        // separate build
        auto mb2 = swctx.createBuild();
        Build b(main_build.swctx, mb2, driver);
        b.getChildren() = swctx.getPredefinedTargets();
        //auto r = b.build_configs_separate({ fn });
        auto ep = b.build_configs1(Files{ fn });
        //b.execute();
        // set our main target
        mb2.getTargetsToBuild()[*ep->tgt] = mb2.getTargets()[*ep->tgt];
        mb2.loadPackages();
        mb2.prepare();
        mb2.execute();
        //mb2.build();
        return ep->r.begin()->second;
    }
    case FrontendType::Cppan:
        // no need to build
        break;
    }
    return {};
}

void Build::load_packages(const PackageIdSet &pkgsids)
{
    std::unordered_set<LocalPackage> in_pkgs;
    for (auto &p : pkgsids)
        in_pkgs.emplace(swctx.getLocalStorage(), p);

    // make pkgs unique
    std::unordered_map<PackageVersionGroupNumber, LocalPackage> cfgs2;
    for (auto &p : in_pkgs)
        cfgs2.emplace(p.getData().group_number, p);

    std::unordered_set<LocalPackage> pkgs;
    for (auto &[gn, p] : cfgs2)
        pkgs.insert(p);

    path dll;
    {
        auto mb2 = main_build.swctx.createBuild();
        Build b(main_build.swctx, mb2, driver); // cache?
        b.getChildren() = main_build.swctx.getPredefinedTargets();
        //dll = b.build_configs(pkgs);
        auto ep = b.build_configs1(pkgs);
        //b.execute();
        // set our main target
        mb2.getTargetsToBuild()[*ep->tgt] = mb2.getTargets()[*ep->tgt];
        mb2.loadPackages();
        mb2.prepare();
        mb2.execute();
        //mb2.build();
        dll = ep->out;
    }

    for (auto &p : in_pkgs)
    {
        auto ep = std::make_shared<NativeModuleTargetEntryPoint>(*this,
            Module(swctx.getModuleStorage().get(dll), gn2suffix(p.getData().group_number)));
        ep->module_data.NamePrefix = p.ppath.slice(0, p.getData().prefix);
        ep->module_data.current_gn = p.getData().group_number;
        ep->module_data.current_module = p.toString();
        ep->module_data.known_targets = pkgsids;
        getChildren()[p].setEntryPoint(std::move(ep));
    }
}

void Build::load_spec_file(const path &fn, const std::set<TargetSettings> &settings)
{
    auto dll = build(fn);

    auto fe = selectFrontendByFilename(fn);
    if (!fe)
        throw SW_RUNTIME_ERROR("frontend was not found for file: " + normalize_path(fn));

    LOG_TRACE(logger, "using " << toString(*fe) << " frontend");
    switch (fe.value())
    {
    case FrontendType::Sw:
        load_dll(dll, settings);
        break;
    case FrontendType::Cppan:
        cppan_load();
        break;
    }
}

void Build::load_inline_spec(const path &fn)
{
    load_configless(fn);
}

void Build::load_dir(const path &)
{
    SW_UNIMPLEMENTED;
}

void Build::load_configless(const path &file_or_dir)
{
    SW_UNIMPLEMENTED;
    //load_dll({}, false);

    bool dir = fs::is_directory(file_or_dir);

    Strings comments;
    if (!dir)
    {
        // for generators
        //config = file_or_dir;

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

    /*for (auto &s : settings)
    {
        //current_settings = &s;
        if (!dir)
        {
            //exe += file_or_dir;

            std::exception_ptr eptr;
            for (auto &c : comments)
            {
                try
                {
                    auto root = YAML::Load(c);
                    cppan_load(root, file_or_dir.stem().u8string());
                    break;
                }
                catch (...)
                {
                    eptr = std::current_exception();
                }
            }
            if (eptr)
                std::rethrow_exception(eptr);

            // count non sw targets
            //if (getChildren().size() == 1)
            //{
                //if (auto nt = getChildren().begin()->second.begin()->second->as<NativeCompiledTarget>())
                    //*nt += file_or_dir;
            //}

            TargetsToBuild = getChildren();
        }
        else
        {
            auto &exe = addExecutable(ide_solution_name);
            bool read_deps_from_comments = false;

            if (!read_deps_from_comments)
            {
                SW_UNIMPLEMENTED; // and never was

                //for (auto &[p, d] : getPackageStore().resolved_packages)
                //{
                    //if (d.installed)
                        //exe += std::make_shared<Dependency>(p.toString());
                //}
            }
        }
    }*/
}

void Build::load_dll(const path &dll, const std::set<TargetSettings> &settings)
{
    auto ep = std::make_shared<NativeModuleTargetEntryPoint>(*this, swctx.getModuleStorage().get(dll));
    for (auto &s : settings)
        ep->loadPackages(s, {}); // load all

    for (auto t : ep->module_data.added_targets)
        TargetsToBuild[t->getPackage()].push_back(t->shared_from_this());
}

void Build::call_event(Target &t, CallbackType et)
{
    for (auto &e : events)
    {
        try
        {
            e(t, et);
        }
        catch (const std::bad_cast &e)
        {
            LOG_DEBUG(logger, "bad cast in callback: " << e.what());
        }
    }
}

const StringSet &Build::getAvailableFrontendNames()
{
    static StringSet s = []
    {
        StringSet s;
        for (const auto &t : getAvailableFrontendTypes())
            s.insert(toString(t));
        return s;
    }();
    return s;
}

const std::set<FrontendType> &Build::getAvailableFrontendTypes()
{
    static std::set<FrontendType> s = []
    {
        std::set<FrontendType> s;
        for (const auto &[k, v] : getAvailableFrontends().left)
            s.insert(k);
        return s;
    }();
    return s;
}

const Build::AvailableFrontends &Build::getAvailableFrontends()
{
    static AvailableFrontends m = []
    {
        AvailableFrontends m;
        auto exts = getCppSourceFileExtensions();

        // objc
        exts.erase(".m");
        exts.erase(".mm");

        // top priority
        m.insert({ FrontendType::Sw, "sw.cpp" });
        m.insert({ FrontendType::Sw, "sw.cxx" });
        m.insert({ FrontendType::Sw, "sw.cc" });

        exts.erase(".cpp");
        exts.erase(".cxx");
        exts.erase(".cc");

        // rest
        for (auto &e : exts)
            m.insert({ FrontendType::Sw, "sw" + e });

        // cppan fe
        m.insert({ FrontendType::Cppan, "cppan.yml" });

        return m;
    }();
    return m;
}

const FilesOrdered &Build::getAvailableFrontendConfigFilenames()
{
    static FilesOrdered f = []
    {
        FilesOrdered f;
        for (auto &[k, v] : getAvailableFrontends().left)
            f.push_back(v);
        return f;
    }();
    return f;
}

bool Build::isFrontendConfigFilename(const path &fn)
{
    return !!selectFrontendByFilename(fn);
}

std::optional<FrontendType> Build::selectFrontendByFilename(const path &fn)
{
    auto i = getAvailableFrontends().right.find(fn.filename());
    if (i == getAvailableFrontends().right.end())
        return {};
    return i->get_left();
}

bool Build::skipTarget(TargetScope Scope) const
{
    return false;
}

bool Build::isKnownTarget(const LocalPackage &p) const
{
    return getModuleData().known_targets.empty() ||
        p.ppath.is_loc() || // used by cfg targets and checks
        getModuleData().known_targets.find(p) != getModuleData().known_targets.end();
}

path Build::getSourceDir(const LocalPackage &p) const
{
    return p.getDirSrc2();
}

std::optional<path> Build::getSourceDir(const Source &s, const Version &v) const
{
    auto s2 = s.clone();
    s2->applyVersion(v);
    auto i = source_dirs_by_source.find(s2->getHash());
    if (i == source_dirs_by_source.end())
        return {};
    return i->second;
}

path Build::getTestDir() const
{
    SW_UNIMPLEMENTED;
    //return BinaryDir / "test" / getSettings().getConfig();
}

void Build::addTest(Test &cb, const String &name)
{
    auto dir = getTestDir() / name;
    fs::remove_all(dir); // also makea condition here

    auto &c = *cb.c;
    c.name = "test: [" + name + "]";
    c.always = true;
    c.working_directory = dir;
    SW_UNIMPLEMENTED;
    //c.addPathDirectory(BinaryDir / getSettings().getConfig());
    c.out.file = dir / "stdout.txt";
    c.err.file = dir / "stderr.txt";
    tests.insert(cb.c);
}

Test Build::addTest(const ExecutableTarget &t)
{
    return addTest("test." + std::to_string(tests.size() + 1), t);
}

Test Build::addTest(const String &name, const ExecutableTarget &tgt)
{
    SW_UNIMPLEMENTED;
    /*auto c = tgt.addCommand();
    c << cmd::prog(tgt);
    Test t(c);
    addTest(t, name);
    return t;*/
}

Test Build::addTest()
{
    return addTest("test." + std::to_string(tests.size() + 1));
}

Test Build::addTest(const String &name)
{
    Test cb(swctx);
    addTest(cb, name);
    return cb;
}

TargetMap &Build::getChildren()
{
    return main_build.getTargets();
}

const TargetMap &Build::getChildren() const
{
    return main_build.getTargets();
}

}
