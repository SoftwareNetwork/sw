// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "build.h"

#include "frontend/cppan/yaml.h"
#include "functions.h"
#include "generator/generator.h"
#include "inserts.h"
#include "module.h"
#include "run.h"
#include "suffix.h"
#include "sw_abi_version.h"
#include "sw_context.h"
#include "target/native.h"

#include <sw/builder/execution_plan.h>
#include <sw/builder/file_storage.h>
#include <sw/builder/program.h>
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

static cl::opt<bool> print_graph("print-graph", cl::desc("Print file with build graph"));
cl::opt<int> skip_errors("k", cl::desc("Skip errors"));
static cl::opt<bool> time_trace("time-trace", cl::desc("Record chrome time trace events"));

bool gVerbose;
bool gWithTesting;
path gIdeFastPath;
path gIdeCopyToDir;
int gNumberOfJobs = -1;

std::map<sw::PackagePath, sw::Version> gUserSelectedPackages;

// TODO: add '#pragma sw driver ...'

namespace sw
{

struct NativeTargetEntryPoint : TargetEntryPoint
{
    const Module &m;

    NativeTargetEntryPoint(const Module &m)
        : m(m)
    {
    }

    TargetBaseTypePtr create(const PackageIdSet &pkgs) override
    {
        SW_UNIMPLEMENTED;
    }
};

void TargetMapInternal::create()
{
    if (!ep)
        throw SW_RUNTIME_ERROR("No entry point provided");
    ep->create({});
}

void check_self(Checker &c);

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

namespace detail
{

void EventCallback::operator()(TargetBase &t, CallbackType e)
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

path getProgramFilesX86()
{
    auto e = getenv("programfiles(x86)");
    if (!e)
        throw SW_RUNTIME_ERROR("Cannot get 'programfiles(x86)' env. var.");
    return e;
}

static path getWindowsKitRoot()
{
    auto p = getProgramFilesX86() / "Windows Kits";
    if (fs::exists(p))
        return p;
    throw SW_RUNTIME_ERROR("No Windows Kits available");
}

String getWin10KitDirName()
{
    return "10";
}

static Strings listWindowsKits()
{
    Strings kits;
    auto kr = getWindowsKitRoot();
    for (auto &k : Strings{ getWin10KitDirName(), "8.1", "8.0", "7.1A", "7.0A", "6.0A" })
    {
        auto d = kr / k;
        if (fs::exists(d))
            kits.push_back(k);
    }
    return kits;
}

static path getLatestWindowsKit()
{
    auto allkits = listWindowsKits();
    if (allkits.empty())
        throw SW_RUNTIME_ERROR("No Windows Kits available");
    return allkits[0];
}

static path getWin10KitInspectionDir()
{
    auto kr = getWindowsKitRoot();
    auto dir = kr / getWin10KitDirName() / "Include";
    return dir;
}

static std::set<path> listWindows10Kits()
{
    std::set<path> kits;
    auto dir = getWin10KitInspectionDir();
    for (auto &i : fs::directory_iterator(dir))
    {
        if (fs::is_directory(i))
        {
            auto d = i.path().filename().u8string();
            Version v = d;
            if (v.isVersion())
                kits.insert(d);
        }
    }
    if (kits.empty())
        throw SW_RUNTIME_ERROR("No Windows 10 Kits available");
    return kits;
}

void BuildSettings::init()
{
    if (TargetOS.is(OSType::Windows))
    {
        if (Native.SDK.Root.empty())
            Native.SDK.Root = getWindowsKitRoot();
        if (Native.SDK.Version.empty())
            Native.SDK.Version = getLatestWindowsKit();
        if (Native.SDK.BuildNumber.empty())
        {
            if (TargetOS.Version >= Version(10) && Native.SDK.Version == getWin10KitDirName())
            {
                // take current or the latest version!
                // sometimes current does not work:
                //  on appveyor we have win10.0.14393.0, but no sdk
                //  but we have the latest sdk there: win10.0.17763.0
                auto dir = getWin10KitInspectionDir();
                path cursdk = TargetOS.Version.toString(4);
                path curdir = dir / cursdk;
                // also check for some executable inside our dir
                if (fs::exists(curdir) &&
                    (fs::exists(Native.SDK.getPath("bin") / cursdk / "x64" / "rc.exe") ||
                        fs::exists(Native.SDK.getPath("bin") / cursdk / "x86" / "rc.exe")))
                    Native.SDK.BuildNumber = curdir.filename();
                else
                    Native.SDK.BuildNumber = *listWindows10Kits().rbegin();
            }
        }
    }
    else if (TargetOS.is(OSType::Macos) || TargetOS.is(OSType::IOS))
    {
        if (Native.SDK.Root.empty())
        {
            String sdktype = "macosx";
            if (TargetOS.is(OSType::IOS))
                sdktype = "iphoneos";

            primitives::Command c;
            c.setProgram("xcrun");
            c.arguments.push_back("--sdk");
            c.arguments.push_back(sdktype);
            c.arguments.push_back("--show-sdk-path");
            error_code ec;
            c.execute(ec);
            if (ec)
            {
                LOG_DEBUG(logger, "cannot find " + sdktype + " sdk path using xcrun");
            }
            else
            {
                Native.SDK.Root = boost::trim_copy(c.out.text);
            }
        }
    }
    else if (TargetOS.Type == OSType::Android)
    {
        if (TargetOS.Arch == ArchType::arm)
        {
            if (TargetOS.SubArch == SubArchType::NoSubArch)
                TargetOS.SubArch = SubArchType::ARMSubArch_v7;
        }
    }
}

String BuildSettings::getConfig() const
{
    // TODO: add get real config, lengthy and with all info

    String c;

    addConfigElement(c, toString(TargetOS.Type));
    if (TargetOS.Type == OSType::Android)
        addConfigElement(c, Native.SDK.Version.string());
    addConfigElement(c, toString(TargetOS.Arch));
    if (TargetOS.Arch == ArchType::arm || TargetOS.Arch == ArchType::aarch64)
        addConfigElement(c, toString(TargetOS.SubArch)); // concat with previous?

    addConfigElement(c, toString(Native.LibrariesType));
    if (TargetOS.Type == OSType::Windows && Native.MT)
        addConfigElement(c, "mt");
    addConfigElement(c, toString(Native.ConfigurationType));

    return c;
}

String BuildSettings::getTargetTriplet() const
{
    // See https://clang.llvm.org/docs/CrossCompilation.html

    String target;
    target += toTripletString(TargetOS.Arch);
    if (TargetOS.Arch == ArchType::arm)
        target += toTripletString(TargetOS.SubArch);
    target += "-unknown"; // vendor
    target += "-" + toTripletString(TargetOS.Type);
    if (TargetOS.Type == OSType::Android)
        target += "-android";
    if (TargetOS.Arch == ArchType::arm)
        target += "eabi";
    if (TargetOS.Type == OSType::Android)
        target += Native.SDK.Version.string();
    return target;
}

bool BuildSettings::operator<(const BuildSettings &rhs) const
{
    return std::tie(TargetOS, Native) < std::tie(rhs.TargetOS, rhs.Native);
}

bool BuildSettings::operator==(const BuildSettings &rhs) const
{
    return std::tie(TargetOS, Native) == std::tie(rhs.TargetOS, rhs.Native);
}

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

static void addImportLibrary(const SwContext &swctx, NativeCompiledTarget &t)
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

static std::tuple<FilesOrdered, UnresolvedPackages> getFileDependencies(const SwContext &swctx, const path &p, std::set<PackageVersionGroupNumber> &gns)
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

static std::tuple<FilesOrdered, UnresolvedPackages> getFileDependencies(const SwContext &swctx, const path &in_config_file)
{
    std::set<PackageVersionGroupNumber> gns;
    return getFileDependencies(swctx, in_config_file, gns);
}

static auto build_configs(const SwContext &swctx, const std::unordered_set<LocalPackage> &pkgs)
{
    Build b(swctx); // cache?
    b.execute_jobs = config_jobs;
    b.Local = false;
    b.file_storage_local = false;
    b.is_config_build = true;
    return b.build_configs(pkgs);
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

Build::Build(const SwContext &swctx)
    : swctx(swctx), checker(*this)
{
    //auto ss = createSettings();
    //addSettings(ss);
    //host_settings = &addSettings(ss);

    // canonical makes disk letter uppercase on windows
    setSourceDirectory(swctx.source_dir);
    BinaryDir = SourceDir / SW_BINARY_DIR;
}

Build::Build(const Build &rhs)
    : TargetBase(rhs)
    , swctx(rhs.swctx)
    , silent(rhs.silent)
    //, show_output(rhs.show_output) // don't pass to checks
    //, knownTargets(rhs.knownTargets)
    , source_dirs_by_source(rhs.source_dirs_by_source)
    , fetch_dir(rhs.fetch_dir)
    , with_testing(rhs.with_testing)
    , ide_solution_name(rhs.ide_solution_name)
    , disable_compiler_lookup(rhs.disable_compiler_lookup)
    , config_file_or_dir(rhs.config_file_or_dir)
    , events(rhs.events)
    , file_storage_local(rhs.file_storage_local)
    , command_storage(rhs.command_storage)
    , prefix_source_dir(rhs.prefix_source_dir)
    , is_config_build(rhs.is_config_build)
    , checker(*this)
{
}

Build::~Build() = default;

BuildSettings Build::createSettings() const
{
    BuildSettings ss;
    ss.TargetOS = getHostOs();
    ss.init();
    return ss;
}

const BuildSettings &Build::addSettings(const BuildSettings &ss)
{
    auto i = std::find(settings.begin(), settings.end(), ss);
    if (i == settings.end())
    {
        current_settings = &ss;
        //detectCompilers(*this);
        settings.push_back(ss);
        current_settings = &settings.back();
    }
    else
        current_settings = &*i;
    return getSettings();
}

void Build::detectCompilers()
{
    for (auto &s : settings)
    {
        if (s.activated)
            continue;
        current_settings = &s;
        ::sw::detectCompilers(*this);
        s.activated = true;
    }
}

const OS &Build::getHostOs() const
{
    return swctx.HostOS;
}

/*void Build::addTargetSettings(const String &ppath_regex, const VersionRange &vr, const TargetSettingsDataContainer &c)
{
    auto &d = target_settings[ppath_regex].emplace_back();
    d.r_ppath = ppath_regex;
    d.range = vr;
    d.data = c;
}*/

path Build::getChecksDir() const
{
    return getServiceDir() / "checks";
}

void Build::build_and_resolve(int n_runs)
{
    auto ud = gatherUnresolvedDependencies(n_runs);
    if (ud.empty())
        return;

    if (is_config_build)
    {
        String s;
        for (auto &u : ud)
            s += u.first.toString() + ", ";
        s.resize(s.size() - 2);
        throw SW_RUNTIME_ERROR("Missing config deps, check your build_self script: " + s);
    }

    if (n_runs > 1)
        LOG_ERROR(logger, "You are here for the third time. This is not intended. Failures are imminent.");

    // first round
    UnresolvedPackages pkgs;
    for (auto &[pkg, d] : ud)
    {
        pkgs.insert(pkg);
        LOG_DEBUG(logger, "Unresolved dependency: " << pkg.toString());
    }

    // resolve only deps needed
    auto m = swctx.install(pkgs);

    // after install

    std::unordered_map<PackageVersionGroupNumber, LocalPackage> cfgs2;
    for (auto &[u, p] : m)
    {
        knownTargets.insert(p);
        // gather packages
        cfgs2.emplace(p.getData().group_number, p);
    }

    std::unordered_set<LocalPackage> cfgs;
    for (auto &[gn, s] : cfgs2)
    {
        if (known_cfgs.find(s) == known_cfgs.end() &&
            children.find(s) == children.end())
            cfgs.insert(s);
    }
    known_cfgs.insert(cfgs.begin(), cfgs.end());
    if (cfgs.empty())
        return;

    // all deps must be resolved in the first run!
    if (n_runs > 0)
    {
        LOG_ERROR(logger, "You are here for the second time. This is not intended. Expect failures.");
        for (auto &pkg : pkgs)
            LOG_ERROR(logger, "Unresolved dependency: " << pkg.toString());
    }

    auto dll = ::sw::build_configs(swctx, cfgs);

    for (auto &[u, p] : m)
        children[p].ep = std::make_unique<NativeTargetEntryPoint>(Module(swctx.getModuleStorage().get(dll), gn2suffix(p.getData().group_number)));

    Local = false;

    for (auto &[gn, p] : cfgs2)
    {
        NamePrefix = p.ppath.slice(0, p.getData().prefix);
        current_gn = gn;
        current_module = p.toString();
        sw_check_abi_version(Module(swctx.getModuleStorage().get(dll), gn2suffix(gn)).sw_get_module_abi_version());
        for (auto &s : settings)
        {
            current_settings = &s;
            Module(swctx.getModuleStorage().get(dll), gn2suffix(gn)).check(*this, checker);
            // we can use new (clone of this) solution, then copy known targets
            // to allow multiple passes-builds
            Module(swctx.getModuleStorage().get(dll), gn2suffix(gn)).build(*this);
        }
    }
    current_gn = 0;
    NamePrefix.clear();
    current_module.clear();

    for (auto &[porig, p] : m)
    {
        for (auto &[n, tgts] : getChildren())
        {
            for (auto &[s, t] : tgts)
            {
                if (t->skip)
                    continue;
                if (p == t->getPackage() && ud[porig])
                {
                    ud[porig]->setTarget(*std::static_pointer_cast<NativeTarget>(t).get());
                }
            }
        }
    }

    {
        ud = gatherUnresolvedDependencies();
        UnresolvedPackages pkgs;
        for (auto &[pkg, d] : ud)
            pkgs.insert(pkg);
        swctx.install(pkgs);

        if (ud.empty())
            return;
    }

    // we have unloaded deps, load them
    // they are runtime deps either due to local overridden packages
    // or to unregistered deps in sw - probably something wrong or
    // malicious

    build_and_resolve(n_runs + 1);
}

UnresolvedDependenciesType Build::gatherUnresolvedDependencies(int n_runs) const
{
    UnresolvedDependenciesType deps;
    std::unordered_set<UnresolvedPackage> known;

    for (const auto &[pkg, tgts] : getChildren())
    {
        for (auto &[stngs, t] : tgts)
        {
            if (t->skip)
                continue;
            auto c = t->gatherUnresolvedDependencies();
            if (c.empty())
                continue;

            for (auto &r : known)
                c.erase(r);
            if (c.empty())
                continue;

            std::unordered_set<UnresolvedPackage> known2;
            for (auto &[up, dptr] : c)
            {
                if (swctx.isResolved(up))
                {
                    auto r = swctx.resolve(up);
                    auto i = children.find(r);
                    if (i != children.end())
                    {
                        dptr->setTarget(*std::static_pointer_cast<NativeTarget>(i->second.begin()->second).get());
                        known2.insert(up);
                        continue;
                    }
                }

                auto i = getChildren().find(up);
                if (i != getChildren().end())
                {
                    dptr->setTarget(*std::static_pointer_cast<NativeTarget>(i->second.begin()->second).get());
                    known2.insert(up);
                }
            }

            for (auto &r : known2)
                c.erase(r);
            known.insert(known2.begin(), known2.end());

            deps.insert(c.begin(), c.end());

            if (n_runs && !c.empty())
            {
                String s;
                for (auto &u : c)
                    s += u.first.toString() + ", ";
                s.resize(s.size() - 2);

                LOG_ERROR(logger, pkg.toString() + " unresolved deps on run " << n_runs << ": " + s);
            }
        }
    }
    return deps;
}

void Build::prepare()
{
    //if (prepared)
        //return;

    ScopedTime t;

    // all targets are set stay unchanged from user
    // so, we're ready to some preparation passes

    build_and_resolve();

    // decide if we need cross compilation

    // multipass prepare()
    // if we add targets inside this loop,
    // it will automatically handle this situation
    while (prepareStep())
        ;

    // prepare tests
    /*if (with_testing)
    {
        for (auto &s : solutions)
        {
            for (auto &t : s.tests)
                ;
        }
    }*/

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
    prepareStep(e, fs, next_pass);
    waitAndGet(fs);

    return next_pass;
}

void Build::prepareStep(Executor &e, Futures<void> &fs, std::atomic_bool &next_pass) const
{
    for (const auto &[pkg, tgts] : getChildren())
    {
        for (auto &[stngs, t] : tgts)
        {
            if (t->skip)
                continue;
            fs.push_back(e.push([this, t = std::ref(t), &next_pass]
                {
                    if (prepareStep(t))
                        next_pass = true;
                }));
        }
    }
}

bool Build::prepareStep(const TargetBaseTypePtr &t) const
{
    // try to run as early as possible
    if (t->mustResolveDeps())
        resolvePass(*t, t->gatherDependencies());

    return t->prepare();
}

void Build::resolvePass(const Target &t, const DependenciesType &deps) const
{
    auto host = this;
    bool select_targets = host;
    if (!host)
        host = this;
    for (auto &d : deps)
    {
        auto h = this;
        if (d->isDummy())
            h = host;
        else if (d->isResolved())
        {
            //if (h->getChildren().find(d->getPackage()) == h->getChildren().end() &&
                //h->dummy_children.find(d->getPackage()) != h->dummy_children.end())
            {
                /*if (d->target->Scope != TargetScope::Tool)
                {
                    auto err = "Package: " + t.getPackage().toString() + ": Unresolved package on stage 1: " + d->getPackage().toString();
                    err += " (but target is set to dummy child)";
                    throw SW_LOGIC_ERROR(err);
                }*/
            }
            continue;
        }

        auto i = h->getChildren().find(d->getPackage());
        if (i != h->getChildren().end())
        {
            auto i2 = i->second.find(TargetSettings{ t.getSettings() });
            if (i2 == i->second.end())
                throw SW_RUNTIME_ERROR("no such target: " + d->getPackage().toString());
            auto t2 = std::static_pointer_cast<NativeTarget>(i2->second);
            if (t2)
                d->setTarget(*t2);
            else
                throw SW_RUNTIME_ERROR("bad target cast to NativeTarget during resolve");

            // turn on only needed targets during cc
            //if (select_targets)
                //host->TargetsToBuild[i->second->getPackage()] = i->second;
        }
        // we fail in any case here, no matter if dependency were resolved previously
        else
        {
            // allow dummy scoped tools
            /*auto i = h->dummy_children.find(d->getPackage());
            if (i != h->dummy_children.end())
            {
                if (i->second->Scope != TargetScope::Tool)
                {
                    auto err = "Package: " + t.getPackage().toString() + ": Unresolved package on stage 1: " + d->getPackage().toString();
                    err += " (but target is set to dummy child)";
                    throw SW_LOGIC_ERROR(err);
                }
                auto t = std::static_pointer_cast<NativeTarget>(i->second);
                if (t)
                    d->setTarget(*t);
                else
                    throw SW_RUNTIME_ERROR("bad target cast to NativeTarget during resolve");

                // turn on only needed targets during cc
                if (select_targets)
                    host->TargetsToBuild[i->second->getPackage()] = i->second;
            }
            else*/
            {
                auto err = "Package: " + t.getPackage().toString() + ": Unresolved package on stage 1: " + d->getPackage().toString();
                if (d->target)
                    err += " (but target is set to " + d->target->getPackage().toString() + ")";
                if (auto d = t.getPackage().getOverriddenDir(); d)
                {
                    err += ".\nPackage: " + t.getPackage().toString() + " is overridden locally. "
                        "This means you have new dependency that is not in db.\n"
                        "Run following command in attempt to fix this issue: "
                        "'sw -d " + normalize_path(d.value()) + " -override-remote-package " +
                        t.getPackage().ppath.slice(0, t.getPackage().getData().prefix).toString() + "'";
                }
                throw SW_LOGIC_ERROR(err);
            }
        }
    }
}

void Build::addFirstConfig()
{
    if (!settings.empty())
        return;

    auto ss = createSettings();
    addSettings(ss);
}

/*void Build::findCompiler()
{
    Settings.init();

    if (!disable_compiler_lookup)
        detectCompilers(*this);

    using CompilerVector = std::vector<std::pair<PackageId, CompilerType>>;

    auto activate_one = [this](auto &v) -> ProgramPtr
    {
        auto r = activateProgram(v.first.ppath);
        if (r)
            this->Settings.Native.CompilerType = v.second;
        return r;
    };

    auto activate_all = [&activate_one](const CompilerVector &a)
    {
        return std::all_of(a.begin(), a.end(), [&activate_one](const auto &v)
        {
            return activate_one(v);
        });
    };

    auto activate_array = [&activate_all](const std::vector<CompilerVector> &a)
    {
        return std::any_of(a.begin(), a.end(), [&activate_all](const auto &v)
        {
            auto r = activate_all(v);
            for (auto &v2 : v)
            {
                if (r)
                    LOG_TRACE(logger, "activated " << v2.first.toString() << " successfully");
                else
                    LOG_TRACE(logger, "activate " << v2.first.toString() << " failed");
            }
            return r;
        });
    };

    auto activate_array_or_throw = [&activate_array](const std::vector<CompilerVector> &a, const auto &e)
    {
        if (!activate_array(a))
            throw SW_RUNTIME_ERROR(e);
    };

    static const CompilerVector msvc =
    {
        {{"com.Microsoft.VisualStudio.VC.cl"}, CompilerType::MSVC},
        {{"com.Microsoft.VisualStudio.VC.ml"}, CompilerType::MSVC},
        {{"com.Microsoft.Windows.rc"}, CompilerType::MSVC},
    };

    static const CompilerVector gnu =
    {
        {{"org.gnu.gcc.gpp"}, CompilerType::GNU},
        {{"org.gnu.gcc.gcc"}, CompilerType::GNU},
        //{{"org.gnu.gcc.as"}, CompilerType::GNU},
    };

    static const CompilerVector clang =
    {
        {{"org.LLVM.clangpp"}, CompilerType::Clang },
        {{"org.LLVM.clang"}, CompilerType::Clang},
    };

    static const CompilerVector clangcl =
    {
        {{"org.LLVM.clangcl"},CompilerType::ClangCl }
    };

    static const CompilerVector appleclang =
    {
        {{"com.apple.LLVM.clangpp"}, CompilerType::AppleClang },
        {{"com.apple.LLVM.clang"}, CompilerType::AppleClang},
    };

    switch (Settings.Native.CompilerType)
    {
    case CompilerType::MSVC:
        activate_array_or_throw({ msvc }, "Cannot find msvc toolchain");
        break;
    case CompilerType::Clang:
        activate_array_or_throw({ clang }, "Cannot find clang toolchain");
        break;
    case CompilerType::ClangCl:
        activate_array_or_throw({ clangcl }, "Cannot find clang-cl toolchain");
        break;
    case CompilerType::AppleClang:
        activate_array_or_throw({ appleclang }, "Cannot find clang toolchain");
        break;
    case CompilerType::GNU:
        activate_array_or_throw({ gnu }, "Cannot find gnu toolchain");
        break;
    case CompilerType::UnspecifiedCompiler:
        switch (getHostOs().Type)
        {
        case OSType::Windows:
            activate_array_or_throw({ msvc, clangcl, clang, }, "Try to add more compilers");
            break;
        case OSType::Cygwin:
        case OSType::Linux:
            activate_array_or_throw({ gnu, clang, }, "Try to add more compilers");
            break;
        case OSType::Macos:
            activate_array_or_throw({ clang, appleclang, gnu, }, "Try to add more compilers");
            break;
        }
        break;
    default:
        throw SW_RUNTIME_ERROR("solution.cpp: not implemented");
    }

    // before linkers
    if (isClangFamily(Settings.Native.CompilerType))
    {
        if (auto p = getProgram("org.LLVM.ld"))
        {
            if (auto l = p->template as<GNULinker>())
            {
                auto cmd = l->createCommand(swctx);
                cmd->arguments.push_back("-fuse-ld=lld");
                cmd->arguments.push_back("-target");
                cmd->arguments.push_back(Settings.getTargetTriplet());
            }
        }
    }

    // lib/link
    auto activate_lib_link_or_throw = [this](const std::vector<std::tuple<PackagePath, LinkerType>> &a, const auto &e, bool link = false)
    {
        if (!std::any_of(a.begin(), a.end(), [this, &link](const auto &v)
            {
                auto p = getProgram(std::get<0>(v));
                if (p)
                {
                    if (!link)
                        this->Settings.Native.Librarian = std::dynamic_pointer_cast<NativeLinker>(p->clone());
                    else
                        this->Settings.Native.Linker = std::dynamic_pointer_cast<NativeLinker>(p->clone());
                    //this->Settings.Native.LinkerType = std::get<1>(v);
                    LOG_TRACE(logger, "activated " << std::get<0>(v).toString() << " successfully");
                }
                else
                {
                    LOG_TRACE(logger, "activate " << std::get<0>(v).toString() << " failed");
                }
                return p;
            }))
            throw SW_RUNTIME_ERROR(e);
    };

    if (Settings.TargetOS.is(OSType::Windows))
    {
        activate_lib_link_or_throw({
            {{"com.Microsoft.VisualStudio.VC.lib"},LinkerType::MSVC},
            {{"org.gnu.binutils.ar"},LinkerType::GNU},
            {{"org.LLVM.ar"},LinkerType::GNU},
            }, "Try to add more librarians");
        activate_lib_link_or_throw({
            {{"com.Microsoft.VisualStudio.VC.link"},LinkerType::MSVC},
            {{"org.gnu.gcc.ld"},LinkerType::GNU},
            {{"org.LLVM.ld"},LinkerType::GNU},
            }, "Try to add more linkers", true);
    }
    else if (Settings.TargetOS.is(OSType::Macos))
    {
        activate_lib_link_or_throw({
            {{"org.LLVM.ar"},LinkerType::GNU},
            {{"org.gnu.binutils.ar"},LinkerType::GNU},
            }, "Try to add more librarians");
        activate_lib_link_or_throw({
            {{"org.LLVM.ld"},LinkerType::GNU},
            {{"com.apple.LLVM.ld"},LinkerType::GNU},
            {{"org.gnu.gcc.ld"},LinkerType::GNU},
            }, "Try to add more linkers", true);
    }
    else
    {
        activate_lib_link_or_throw({
            // base
            {{"org.gnu.binutils.ar"},LinkerType::GNU},
            {{"org.LLVM.ar"},LinkerType::GNU},
            // cygwin alternative, remove?
            {{"com.Microsoft.VisualStudio.VC.lib"},LinkerType::MSVC},
            }, "Try to add more librarians");
        activate_lib_link_or_throw({
            // base
            {{"org.gnu.gcc.ld"},LinkerType::GNU},
            {{"org.LLVM.ld"},LinkerType::GNU},
            // cygwin alternative, remove?
            {{"com.Microsoft.VisualStudio.VC.link"},LinkerType::MSVC},
            }, "Try to add more linkers", true);
    }

    static const CompilerVector other =
    {
        {{"com.Microsoft.VisualStudio.Roslyn.csc"}, CompilerType::MSVC},
        {{"org.rust.rustc"}, CompilerType::MSVC},
        {{"org.google.golang.go"}, CompilerType::MSVC},
        {{"org.gnu.gcc.fortran"}, CompilerType::MSVC},
        {{"com.oracle.java.javac"}, CompilerType::MSVC},
        {{"com.JetBrains.kotlin.kotlinc"}, CompilerType::MSVC},
        {{"org.dlang.dmd.dmd"}, CompilerType::MSVC},
    };

    // more languages
    for (auto &[a, _] : other)
        activateProgram(a);

    // use activate
    if (!is_config_build)
    {
        for (auto &[pp, v] : gUserSelectedPackages)
        {
            auto prog = getProgram({ pp, v }, false);
            if (!prog)
                throw SW_RUNTIME_ERROR("program is not available: " + pp.toString());

            if (auto vs = prog->as<VSInstance>())
                vs->activate(*this);
        }
    }

    if (Settings.TargetOS.Type != OSType::Macos)
    {
        removeExtension(".m");
        removeExtension(".mm");
    }

    if (isClangFamily(Settings.Native.CompilerType))
    {
        auto add_target = [this](auto & pp)
        {
            auto prog = getProgram(pp);
            if (prog)
            {
                if (auto c = prog->template as<ClangCompiler>())
                {
                    c->Target = Settings.getTargetTriplet();
                }
            }
        };

        add_target("org.LLVM.clang");
        add_target("org.LLVM.clangpp");
    }

    setSettings();
}*/

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
    auto &solution = *this;
    solution.IsConfig = true;
    auto &lib = solution.addTarget<SharedLibraryTarget>(getSelfTargetName(files), "local");
    solution.IsConfig = false;
    return lib;
}

#define SW_DRIVER_NAME "org.sw.sw.client.driver.cpp-0.3.0"
#define SW_DRIVER_INCLUDE_DIR "src"

static NativeCompiledTarget &getDriverTarget(Build &solution)
{
    auto i = solution.getChildren().find(UnresolvedPackage(SW_DRIVER_NAME));
    if (i == solution.getChildren().end())
        throw SW_RUNTIME_ERROR("no driver target");
    TargetSettings tid{ solution.getSettings() };
    auto k = i->second.find(tid);
    if (k == i->second.end())
        throw SW_RUNTIME_ERROR("no driver target (by settings)");
    return *std::dynamic_pointer_cast<NativeCompiledTarget>(k->second);
}

static void addDeps(NativeCompiledTarget &lib, Build &solution)
{
    lib += "pub.egorpugin.primitives.templates-master"_dep; // for SW_RUNTIME_ERROR

    // uncomment when you need help
    //lib += "pub.egorpugin.primitives.source-master"_dep;
    //lib += "pub.egorpugin.primitives.version-master"_dep;
    //lib += "pub.egorpugin.primitives.command-master"_dep;
    //lib += "pub.egorpugin.primitives.filesystem-master"_dep;

    auto &drv = getDriverTarget(solution);
    auto d = lib + drv;
    d->IncludeDirectoriesOnly = true;

    // generated file
    lib += drv.BinaryDir / "options_cl.generated.h";
}

// add Dirs?
static path getDriverIncludeDir(Build &solution)
{
    return getDriverTarget(solution).SourceDir / SW_DRIVER_INCLUDE_DIR;
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

path Build::getOutputModuleName(const path &p)
{
    SW_UNIMPLEMENTED;

    /*addFirstConfig();

    auto &solution = solutions[0];

    solution.Settings.Native.LibrariesType = LibraryType::Static;
    if (debug_configs)
        solution.Settings.Native.ConfigurationType = ConfigurationType::Debug;
    auto &lib = createTarget({ p });
    return lib.getOutputFile();*/
}

const BuildSettings &Build::getSettings() const
{
    if (!current_settings)
        throw SW_LOGIC_ERROR("no settings was set");
    return *current_settings;
}

FilesMap Build::build_configs_separate(const Files &files)
{
    FilesMap r;
    if (files.empty())
        return r;

    addFirstConfig();

    settings[0].Native.LibrariesType = LibraryType::Static;
    if (debug_configs)
        settings[0].Native.ConfigurationType = ConfigurationType::Debug;

    detectCompilers();

    bool once = false;
    auto prepare_config = [this, &once](const auto &fn)
    {
        auto &lib = createTarget({ fn });

        // must check is changed?
        if (do_not_rebuild_config && fs::exists(lib.getOutputFile()))
            return lib.getOutputFile();

        do_not_rebuild_config = false;

        if (!once)
        {
            check_self(checker);
            build_self();
            addDeps(lib, *this);
            once = true;
        }

        addImportLibrary(swctx, lib);
        lib.AutoDetectOptions = false;
        lib.CPPVersion = CPPLanguageStandard::CPP17;
        if (lib.getSettings().TargetOS.is(OSType::Windows))
            lib += "_CRT_SECURE_NO_WARNINGS"_def;

        lib += fn;
        write_pch(*this);
        PrecompiledHeader pch;
        pch.header = getDriverIncludeDir(*this) / getMainPchFilename();
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
                c->ForcedIncludeFiles().push_back(getDriverIncludeDir(*this) / getSw1Header());
                c->ForcedIncludeFiles().push_back(getDriverIncludeDir(*this) / getSwCheckAbiVersionHeader());

                // deprecated warning
                c->Warnings().TreatAsError.push_back(4996);
            }
            else if (auto c = sf->compiler->template as<ClangClCompiler>())
            {
                c->ForcedIncludeFiles().push_back(getDriverIncludeDir(*this) / getSw1Header());
                c->ForcedIncludeFiles().push_back(getDriverIncludeDir(*this) / getSwCheckAbiVersionHeader());
            }
            else if (auto c = sf->compiler->template as<ClangCompiler>())
            {
                //c->ForcedIncludeFiles().push_back(getDriverIncludeDir(solution) / getSw1Header());
                c->ForcedIncludeFiles().push_back(getDriverIncludeDir(*this) / getSwCheckAbiVersionHeader());
            }
            else if (auto c = sf->compiler->template as<GNUCompiler>())
            {
                //c->ForcedIncludeFiles().push_back(getDriverIncludeDir(solution) / getSw1Header());
                c->ForcedIncludeFiles().push_back(getDriverIncludeDir(*this) / getSwCheckAbiVersionHeader());
            }
        }

        if (getSettings().TargetOS.is(OSType::Windows))
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

        if (getSettings().TargetOS.is(OSType::Windows))
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

        auto i = this->children.find(lib.getPackage());
        if (i == this->children.end())
            throw std::logic_error("config target not found");
        this->TargetsToBuild[i->first] = i->second;

        return lib.getOutputFile();
    };

    for (auto &fn : files)
        r[fn] = prepare_config(fn);

    if (!do_not_rebuild_config)
    {
        //Solution::execute();
        execute();
    }

    return r;
}

path Build::build_configs(const std::unordered_set<LocalPackage> &pkgs)
{
    if (pkgs.empty())
        return {};

    bool init = false;
    if (settings.empty())
    {
        addFirstConfig();

        settings[0].Native.LibrariesType = LibraryType::Static;
        if (debug_configs)
            settings[0].Native.ConfigurationType = ConfigurationType::Debug;

        detectCompilers();

        init = true;
    }

    auto &solution = *this;

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

    auto &lib = createTarget(files);

    SCOPE_EXIT
    {
        solution.children.erase(lib.getPackage());
    };

    // must check is changed?
    if (do_not_rebuild_config && fs::exists(lib.getOutputFile()))
        return lib.getOutputFile();

    do_not_rebuild_config = false;

    if (init)
    {
        check_self(solution.checker);
        build_self();
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
        lib[fn].as<NativeSourceFile>()->setOutputFile(lib, fn.u8string() + "." + getCurrentModuleId(), lib.getObjectDir(pkg) / "self");
        if (gVerbose)
            lib[fn].fancy_name += " (" + normalize_path(fn) + ")";
    }

    //
    write_pch(solution);
    PrecompiledHeader pch;
    pch.header = getDriverIncludeDir(solution) / getMainPchFilename();
    pch.source = getImportPchFile(swctx);
    pch.force_include_pch = true;
    pch.force_include_pch_to_source = true;
    lib.addPrecompiledHeader(pch);

    auto gnu_setup = [this, &solution](auto *c, const auto &headers, const path &fn, LocalPackage &pkg)
    {
        // we use pch, but cannot add more defs on CL
        // so we create a file with them
        auto gn = pkg.getData().group_number;
        auto hash = gn2suffix(gn);
        path h;
        // cannot create aux dir on windows; auxl = auxiliary
        if (is_under_root(fn, swctx.getLocalStorage().storage_dir_pkg))
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
        c->ForcedIncludeFiles().push_back(getDriverIncludeDir(solution) / getSw1Header());

        for (auto &h : headers)
            c->ForcedIncludeFiles().push_back(h);
        c->ForcedIncludeFiles().push_back(getDriverIncludeDir(solution) / getSwCheckAbiVersionHeader());
    };

    for (auto &[fn, pkg] : output_names)
    {
        auto [headers, udeps] = getFileDependencies(swctx, fn);
        if (auto sf = lib[fn].template as<NativeSourceFile>())
        {
            if (auto c = sf->compiler->template as<VisualStudioCompiler>())
            {
                gnu_setup(c, headers, fn, pkg);
            }
            else if (auto c = sf->compiler->template as<ClangClCompiler>())
            {
                gnu_setup(c, headers, fn, pkg);
            }
            else if (auto c = sf->compiler->template as<ClangCompiler>())
            {
                gnu_setup(c, headers, fn, pkg);
            }
            else if (auto c = sf->compiler->template as<GNUCompiler>())
            {
                gnu_setup(c, headers, fn, pkg);
            }
        }
        for (auto &d : udeps)
            lib += std::make_shared<Dependency>(d);
    }

    if (settings[0].TargetOS.is(OSType::Windows))
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

    if (settings[0].TargetOS.is(OSType::Windows))
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

    execute();

    return lib.getOutputFile();
}

// can be used in configs to load subdir configs
// s.build->loadModule("client/sw.cpp").call<void(Solution &)>("build", s);
Module Build::loadModule(const path &p) const
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
    return swctx.getModuleStorage().get(dll);
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
            (File(fn, swctx.getServiceFileStorage()).isChanged() || File(dll, swctx.getServiceFileStorage()).isChanged()))
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
    {
        fetch_dir = BinaryDir / "src";
    }

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
}

static Build::CommandExecutionPlan load(const SwContext &swctx, const path &fn, const Build &s)
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
                auto c2 = std::make_shared<ExecuteBuiltinCommand>(swctx);
                c = c2;
            }
                break;
            default:
                c = std::make_shared<builder::Command>(swctx);
                break;
            }
            commands[id] = c;
            c->fs = &swctx.getServiceFileStorage();
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

        c->setProgram(read_string());
        c->working_directory = read_string();

        size_t n;
        ctx.read(n);
        while (n--)
            c->arguments.push_back(read_string());

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
    return Build::CommandExecutionPlan::createExecutionPlan(commands2);
}

void save(const path &fn, const Build::CommandExecutionPlan &p)
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
        else if (auto c2 = c->as<ExecuteBuiltinCommand>(); c2)
        {
            type = 3;
            ctx.write(type);
        }
        else
            ctx.write(type);

        print_string(c->getName());

        print_string(c->working_directory.u8string());

        ctx.write(c->arguments.size());
        for (auto &a : c->arguments)
            print_string(a->toString());

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

path Build::getIdeDir() const
{
    //const auto compiler_name = boost::to_lower_copy(toString(getSettings().Native.CompilerType));
    const auto compiler_name = "msvc";
    return BinaryDir / "sln" / ide_solution_name / compiler_name;
}

path Build::getExecutionPlansDir() const
{
    return getIdeDir().parent_path() / "explans";
}

path Build::getExecutionPlanFilename() const
{
    String n;
    for (auto &[pkg, _] : TargetsToBuild)
        n += pkg.toString();
    return getExecutionPlansDir() / (getSettings().getConfig() + "_" + sha1(n).substr(0, 8) + ".explan");
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

        auto fn = getExecutionPlanFilename();
        if (fs::exists(fn))
        {
            // prevent double assign generators
            swctx.getServiceFileStorage().reset();

            auto p = ::sw::load(swctx, fn, *this);
            execute(p);
            return;
        }
    }

    prepare();

    if (ide)
    {
        // write execution plans
        auto p = getExecutionPlan();
        auto fn = getExecutionPlanFilename();
        if (!fs::exists(fn))
            save(fn, p);
    }

    if (getGenerator())
    {
        generateBuildSystem();
        return;
    }

    prepare();
    auto p = getExecutionPlan();
    execute(p);

    if (with_testing)
    {
        Commands cmds;
        cmds.insert(tests.begin(), tests.end());
        auto p = getExecutionPlan(cmds);
        execute(p);
    }
}

void Build::execute(CommandExecutionPlan &p) const
{
    auto print_graph = [](const auto &ep, const path &p, bool short_names = false)
    {
        String s;
        s += "digraph G {\n";
        for (auto &c : ep.commands)
        {
            {
                s += c->getName(short_names) + ";\n";
                for (auto &d : c->dependencies)
                    s += c->getName(short_names) + " -> " + d->getName(short_names) + ";\n";
            }
            /*s += "{";
            s += "rank = same;";
            for (auto &c : level)
            s += c->getName(short_names) + ";\n";
            s += "};";*/
        }

        /*if (ep.Root)
        {
        const auto root_name = "all"s;
        s += root_name + ";\n";
        for (auto &d : ep.Root->dependencies)
        s += root_name + " -> " + d->getName(short_names) + ";\n";
        }*/

        s += "}";
        write_file(p, s);
    };

    for (auto &c : p.commands)
    {
        c->silent = silent;
        c->show_output = show_output;
    }

    // execute early to prevent commands expansion into response files
    // print misc
    if (::print_graph && !silent) // && !b console mode
    {
        auto d = getServiceDir();

        //message_box(d.string());

        // new graphs
        //p.printGraph(p.getGraphSkeleton(), d / "build_skeleton");
        p.printGraph(p.getGraph(), d / "build");

        // old graphs
        print_graph(p, d / "build_old.dot");

        if (auto b = this->template as<Build>(); b)
        {
            SW_UNIMPLEMENTED;
            //for (const auto &[i, s] : enumerate(b->solutions))
                //s.printGraph(d / ("solution." + std::to_string(i + 1) + ".dot"));
        }
    }

    if (dry_run)
        return;

    ScopedTime t;
    std::unique_ptr<Executor> ex;
    if (execute_jobs > 0)
        ex = std::make_unique<Executor>(execute_jobs);
    auto &e = execute_jobs > 0 ? *ex : getExecutor();

    // prevent memory leaks (high mem usage)
    /*updateConcurrentContext();
    for (int i = 0; i < 1000; i++)
    e.push([] {updateConcurrentContext(); });*/

    p.skip_errors = skip_errors.getValue();
    p.execute(e);
    auto t2 = t.getTimeFloat();
    if (!silent && t2 > 0.15)
        LOG_INFO(logger, "Build time: " << t2 << " s.");

    // produce chrome tracing log
    if (time_trace)
    {
        // calculate minimal time
        auto min = decltype (builder::Command::t_begin)::clock::now();
        for (auto &c : p.commands)
        {
            if (c->t_begin.time_since_epoch().count() == 0)
                continue;
            min = std::min(c->t_begin, min);
        }

        auto tid_to_ll = [](auto &id)
        {
            std::ostringstream ss;
            ss << id;
            return ss.str();
        };

        nlohmann::json trace;
        nlohmann::json events;
        for (auto &c : p.commands)
        {
            if (c->t_begin.time_since_epoch().count() == 0)
                continue;

            nlohmann::json b;
            b["name"] = c->getName();
            b["cat"] = "BUILD";
            b["pid"] = 1;
            b["tid"] = tid_to_ll(c->tid);
            b["ts"] = std::chrono::duration_cast<std::chrono::microseconds>(c->t_begin - min).count();
            b["ph"] = "B";
            events.push_back(b);

            nlohmann::json e;
            e["name"] = c->getName();
            e["cat"] = "BUILD";
            e["pid"] = 1;
            e["tid"] = tid_to_ll(c->tid);
            e["ts"] = std::chrono::duration_cast<std::chrono::microseconds>(c->t_end - min).count();
            e["ph"] = "E";
            events.push_back(e);
        }
        trace["traceEvents"] = events;
        write_file(getServiceDir() / "time_trace.json", trace.dump(2));
    }

    // prevent memory leaks (high mem usage)
    /*updateConcurrentContext();
    for (int i = 0; i < 1000; i++)
    e.push([] {updateConcurrentContext(); });*/
}

Commands Build::getCommands() const
{
    // calling this in any case to set proper command dependencies
    for (const auto &[pkg, tgts] : getChildren())
    {
        for (auto &[stngs, t] : tgts)
        {
            if (t->skip)
                continue;
            for (auto &c : t->getCommands())
                c->maybe_unused = builder::Command::MU_TRUE;
        }
    }

    Commands cmds;
    // FIXME: drop children from here, always build only precisely picked TargetsToBuild
    auto &chldr = TargetsToBuild.empty() ? getChildren() : TargetsToBuild;
    //if (TargetsToBuild.empty())
        //LOG_WARN("logger", "empty TargetsToBuild");

    for (auto &[p, tgts] : chldr)
    {
        for (auto &[stngs, t] : tgts)
        {
            if (t->skip)
                continue;
            auto c = t->getCommands();
            for (auto &c2 : c)
                c2->maybe_unused &= ~builder::Command::MU_TRUE;
            cmds.insert(c.begin(), c.end());

            // copy output dlls

            auto nt = t->as<NativeCompiledTarget>();
            if (!nt)
                continue;
            if (*nt->HeaderOnly)
                continue;
            if (nt->getSelectedTool() == nt->Librarian.get())
                continue;

            // copy
            if (nt->isLocal() && //getSettings().Native.CopySharedLibraries &&
                nt->Scope == TargetScope::Build && nt->OutputDir.empty() && !nt->createWindowsRpath())
            {
                for (auto &l : nt->gatherAllRelatedDependencies())
                {
                    auto dt = l->as<NativeCompiledTarget>();
                    if (!dt)
                        continue;
                    if (dt->isLocal())
                        continue;
                    if (dt->HeaderOnly.value())
                        continue;
                    if (dt->getSettings().Native.LibrariesType != LibraryType::Shared && !dt->isSharedOnly())
                        continue;
                    if (dt->getSelectedTool() == dt->Librarian.get())
                        continue;
                    auto in = dt->getOutputFile();
                    auto o = nt->getOutputDir() / dt->OutputDir;
                    o /= in.filename();
                    if (in == o)
                        continue;

                    SW_MAKE_EXECUTE_BUILTIN_COMMAND(copy_cmd, *nt, "sw_copy_file", nullptr);
                    copy_cmd->arguments.push_back(in.u8string());
                    copy_cmd->arguments.push_back(o.u8string());
                    copy_cmd->addInput(dt->getOutputFile());
                    copy_cmd->addOutput(o);
                    copy_cmd->dependencies.insert(nt->getCommand());
                    copy_cmd->name = "copy: " + normalize_path(o);
                    copy_cmd->maybe_unused = builder::Command::MU_ALWAYS;
                    copy_cmd->command_storage = builder::Command::CS_LOCAL;
                    cmds.insert(copy_cmd);
                }
            }
        }
    }

    return cmds;
}

Build::CommandExecutionPlan Build::getExecutionPlan() const
{
    return getExecutionPlan(getCommands());
}

Build::CommandExecutionPlan Build::getExecutionPlan(const Commands &cmds) const
{
    auto ep = CommandExecutionPlan::createExecutionPlan(cmds);
    if (ep)
        return ep;

    // error!

    auto d = getServiceDir();

    auto [g, n, sc] = ep.getStrongComponents();

    using Subgraph = boost::subgraph<CommandExecutionPlan::Graph>;

    // fill copy of g
    Subgraph root(g.m_vertices.size());
    for (auto &e : g.m_edges)
        boost::add_edge(e.m_source, e.m_target, root);

    std::vector<Subgraph*> subs(n);
    for (decltype(n) i = 0; i < n; i++)
        subs[i] = &root.create_subgraph();
    for (int i = 0; i < sc.size(); i++)
        boost::add_vertex(i, *subs[sc[i]]);

    auto cyclic_path = d / "cyclic";
    fs::create_directories(cyclic_path);
    for (decltype(n) i = 0; i < n; i++)
    {
        if (subs[i]->m_graph.m_vertices.size() > 1)
            CommandExecutionPlan::printGraph(subs[i]->m_graph, cyclic_path / std::to_string(i));
    }

    ep.printGraph(ep.getGraph(), cyclic_path / "processed", ep.commands, true);
    ep.printGraph(ep.getGraphUnprocessed(), cyclic_path / "unprocessed", ep.unprocessed_commands, true);

    //String error = "Cannot create execution plan because of cyclic dependencies: strong components = " + std::to_string(n);
    String error = "Cannot create execution plan because of cyclic dependencies";

    throw SW_RUNTIME_ERROR(error);
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
            auto e = f.find("*//*", b);
            if (e != f.npos)
            {
                auto s = f.substr(b + 2, e - b - 2);
                if (!s.empty())
                    comments.push_back(s);
            }
        }
    }

    createSolutions("", false);
    for (auto &s : settings)
    {
        current_settings = &s;
        if (!dir)
        {
            //exe += file_or_dir;

            for (auto &c : comments)
            {
                auto root = YAML::Load(c);
                cppan_load(root, file_or_dir.stem().u8string());
            }

            // count non sw targets
            SW_UNIMPLEMENTED;
            /*if (s.children.size() == 1)
            {
                if (auto nt = s.children.begin()->second->as<NativeCompiledTarget>())
                    *nt += file_or_dir;
            }

            TargetsToBuild = s.children;*/
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
    }
}

void Build::generateBuildSystem()
{
    if (!getGenerator())
        return;

    getCommands();
    getExecutionPlan(); // also prepare commands

    for (auto &s : settings)
    {
        current_settings = &s;
        fs::remove_all(getExecutionPlanFilename());
    }
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
            settings.clear();
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
    for (auto &[u, p] : m)
        children[p].ep = std::make_unique<NativeTargetEntryPoint>(Module(swctx.getModuleStorage().get(dll), gn2suffix(p.getData().group_number)));

    createSolutions(dll, true);

    for (auto &[gn, p] : cfgs2)
    {
        NamePrefix = p.ppath.slice(0, p.getData().prefix);
        current_gn = gn;
        current_module = p.toString();
        sw_check_abi_version(Module(swctx.getModuleStorage().get(dll), gn2suffix(gn)).sw_get_module_abi_version());
        for (auto &s : settings)
        {
            current_settings = &s;
            Module(swctx.getModuleStorage().get(dll), gn2suffix(gn)).check(*this, checker);
            // we can use new (clone of this) solution, then copy known targets
            // to allow multiple passes-builds
            Module(swctx.getModuleStorage().get(dll), gn2suffix(gn)).build(*this);
        }
    }
    current_gn = 0;
    NamePrefix.clear();
    current_module.clear();

    // clear TargetsToBuild that is set before
    TargetsToBuild.clear();

    // now we set ours TargetsToBuild to this object
    // execute() will propagate them to solutions
    for (auto &[porig, p] : m)
        TargetsToBuild[p] = getChildren()[p];
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
            auto i = getChildren().find(p);
            if (i == getChildren().end())
                throw SW_RUNTIME_ERROR("No such target in fast path: " + p.toString());
            if (auto nt = i->second.begin()->second->as<NativeCompiledTarget>())
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
                        if (dt->getSettings().Native.LibrariesType != LibraryType::Shared && !dt->isSharedOnly())
                            continue;
                        auto in = dt->getOutputFile();
                        auto o = gIdeCopyToDir / dt->OutputDir;
                        o /= in.filename();
                        if (in == o)
                            continue;

                        SW_MAKE_EXECUTE_BUILTIN_COMMAND(copy_cmd, *nt, "sw_copy_file", nullptr);
                        copy_cmd->arguments.push_back(in.u8string());
                        copy_cmd->arguments.push_back(o.u8string());
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
        getExecutionPlan(cmds).execute(getExecutor());

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
    SW_UNIMPLEMENTED;

    /*build_packages({ s });

    auto nt = solutions[0].getTargetPtr(swctx.resolve(extractFromString(s)))->as<NativeCompiledTarget>();
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

    run(nt->getPackage(), *cb.c);*/
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

    //if (usedll)
        //sw_check_abi_version(Module(swctx.getModuleStorage().get(dll)).sw_get_module_abi_version());

    // configure may change defaults, so we must care below
    if (usedll && configure)
        Module(swctx.getModuleStorage().get(dll)).configure(*this);

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
        addFirstConfig();

        auto times = [this](int n)
        {
            if (n <= 1)
                return;
            auto s2 = settings;
            for (int i = 1; i < n; i++)
            {
                for (auto &s : s2)
                    settings.push_back(s);
            }
        };

        auto mult_and_action = [this, &times](int n, auto f)
        {
            times(n);
            for (int i = 0; i < n; i++)
            {
                int mult = settings.size() / n;
                for (int j = i * mult; j < (i + 1) * mult; j++)
                    f(*(settings.begin() + j), i);
            }
        };

        // configuration
        auto set_conf = [this](auto &s, const String &configuration)
        {
            auto t = configurationTypeFromStringCaseI(configuration);
            if (toIndex(t))
                s.Native.ConfigurationType = t;
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
                    s.Native.LibrariesType = LibraryType::Static;
                if (i == 1)
                    s.Native.LibrariesType = LibraryType::Shared;
            });
        }
        else
        {
            for (auto &s : settings)
            {
                if (static_build)
                    s.Native.LibrariesType = LibraryType::Static;
                if (shared_build)
                    s.Native.LibrariesType = LibraryType::Shared;
            }
        }

        // mt/md
        if (win_mt && win_md)
        {
            mult_and_action(2, [&set_conf](auto &s, int i)
            {
                if (i == 0)
                    s.Native.MT = true;
                if (i == 1)
                    s.Native.MT = false;
            });
        }
        else
        {
            for (auto &s : settings)
            {
                if (win_mt)
                    s.Native.MT = true;
                if (win_md)
                    s.Native.MT = false;
            }
        }

        // platform
        auto set_pl = [](auto &s, const String &platform)
        {
            auto t = archTypeFromStringCaseI(platform);
            if (toIndex(t))
                s.TargetOS.Arch = t;
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
                s.Native.CompilerType1 = t;
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
                s.TargetOS.Type = t;
        };

        mult_and_action(target_os.size(), [&set_tos](auto &s, int i)
        {
            set_tos(s, target_os[i]);
        });

        // libc
        //auto set_libc = [](auto &s, const String &libc)
        //{
        //    s.Settings.Native.libc = libc;
        //};

        //mult_and_action(libc.size(), [&set_libc](auto &s, int i)
        //{
        //    set_libc(s, libc[i]);
        //});
    }
    else if (auto g = getGenerator())
    {
        g->createSolutions(*this);
    }

    // one more time, if generator did not add solution or whatever
    addFirstConfig();

    // finally
    detectCompilers();
}

void Build::load_dll(const path &dll, bool usedll)
{
    createSolutions(dll, usedll);

    // add cc if needed
    //getHostSolution();

    // initiate libc
    for (auto &s : settings)
    {
        current_settings = &s;
        //if (s.Settings.Native.libc)
        //{
        //    Resolver r;
        //    r.resolve_dependencies({ *s.Settings.Native.libc });
        //    auto dd = r.getDownloadDependencies();
        //    for (auto &p : dd)
        //        s.knownTargets.insert(p);

        //    // gather packages
        //    std::unordered_set<ExtendedPackageData> cfgs;
        //    for (auto &[p, _] : r.getDownloadDependenciesWithGroupNumbers())
        //        cfgs.insert(p);

        //    auto dll = ::sw::build_configs(cfgs);
        //    auto b = *this;
        //    b.solutions[i].Settings.Native.libc.reset();
        //    b.load_dll(dll);

        //    // copy back prepared programs (compilers, linkers etc.)
        //    (LanguageStorage&)s = (LanguageStorage&)b.solutions[i];
        //}
    }

    // detect and eliminate solution clones?

    if (auto g = getGenerator())
    {
        g->initSolutions(*this);
    }

    // print info
    if (auto g = getGenerator())
    {
        LOG_INFO(logger, "Generating " << toString(g->type) << " project with " << settings.size() << " configurations:");
        for (auto &s : settings)
            LOG_INFO(logger, s.getConfig());
    }
    else
    {
        LOG_DEBUG(logger, (getGenerator() ? "Generating " + toString(getGenerator()->type) + " " : "Building ")
            << "project with " << settings.size() << " configurations:");
        for (auto &s : settings)
            LOG_DEBUG(logger, s.getConfig());
    }

    // check
    {
        // some packages want checks in their build body
        // because they use variables from checks

        // make parallel?
        if (usedll)
        {
            for (auto &s : settings)
            {
                current_settings = &s;
                Module(swctx.getModuleStorage().get(dll)).check(*this, checker);
            }
        }
    }

    // build
    if (usedll)
    {
        for (const auto &[i,s] : enumerate(settings))
        {
            if (settings.size() > 1)
                LOG_INFO(logger, "[" << (i + 1) << "/" << settings.size() << "] load pass " << s.getConfig());
            current_settings = &s;
            Module(swctx.getModuleStorage().get(dll)).build(*this);
        }
    }

    // we build only targets from this package
    // for example, on linux we do not build skipped windows projects
    /*for (auto &s : settings)
    {
        // only exception is cc host solution
        if (getHostSolution() == &s)
            continue;
        s.TargetsToBuild = s.children;
    }*/
}

/*const Solution *Build::getHostSolution() const
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
        return !s.getHostOs().canRunTargetExecutables(s.Settings.TargetOS);
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
}*/

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

void Build::call_event(TargetBase &t, CallbackType et)
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
        m.insert({ FrontendType::Sw, "sw.cpp" });
        m.insert({ FrontendType::Sw, "sw.cc" });
        m.insert({ FrontendType::Sw, "sw.cxx" });
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
    if (Scope == TargetScope::Test ||
        Scope == TargetScope::UnitTest
        )
        return !with_testing;
    return false;
}

bool Build::isKnownTarget(const LocalPackage &p) const
{
    return knownTargets.empty() ||
        p.ppath.is_loc() ||
        knownTargets.find(p) != knownTargets.end();
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

PackageDescriptionMap Build::getPackages() const
{
    PackageDescriptionMap m;

    for (auto &[pkg, tgts] : getChildren())
    {
        // deps
        if (pkg.ppath.isAbsolute())
            continue;

        auto &t = tgts.begin()->second;
        if (t->sw_provided)
            continue;
        if (t->skip)
            continue;

        // do not participate in build
        if (t->Scope != TargetScope::Build)
            continue;

        nlohmann::json j;

        // source, version, path
        t->getSource().save(j["source"]);
        j["version"] = pkg.getVersion().toString();
        j["path"] = pkg.ppath.toString();

        auto rd = SourceDir;
        if (!fetch_info.sources.empty())
        {
            auto src = t->getSource().clone(); // copy
            src->applyVersion(t->getPackage().version);
            auto si = fetch_info.sources.find(src->getHash());
            if (si == fetch_info.sources.end())
                throw SW_RUNTIME_ERROR("no such source");
            rd = si->second;
        }
        j["root_dir"] = normalize_path(rd);

        // files
        // we do not use nt->gatherSourceFiles(); as it removes deleted files
        Files files;
        for (auto &f : t->gatherAllFiles())
        {
            if (File(f, t->getFs()).isGeneratedAtAll())
                continue;
            files.insert(f.lexically_normal());
        }

        if (auto nt = t->as<NativeCompiledTarget>())
        {
            // TODO: BUG: interface files are not gathered!
            if (files.empty() && !nt->Empty)
                throw SW_RUNTIME_ERROR(pkg.toString() + ": No files found");
            if (!files.empty() && nt->Empty)
                throw SW_RUNTIME_ERROR(pkg.toString() + ": Files were found, but target is marked as empty");
        }

        // we put files under SW_SDIR_NAME to keep space near it
        // e.g. for patch dir or other dirs (server provided files)
        // we might unpack to other dir, but server could push service files in neighbor dirs like gpg keys etc
        nlohmann::json jm;
        auto files_map1 = primitives::pack::prepare_files(files, rd.lexically_normal());
        for (const auto &[f1, f2] : files_map1)
        {
            nlohmann::json jf;
            jf["from"] = normalize_path(f1);
            jf["to"] = normalize_path(f2);
            if (!prefix_source_dir.empty() && f2.u8string().find(prefix_source_dir.u8string()) == 0)
            {
                auto t = normalize_path(f2);
                t = t.substr(prefix_source_dir.u8string().size());
                if (!t.empty() && t.front() == '/')
                    t = t.substr(1);
                jf["to"] = t;
            }
            j["files"].push_back(jf);
        }

        // deps
        for (auto &d : t->gatherDependencies())
        {
            if (d->target && d->target->Scope != TargetScope::Build)
                continue;
            if (d->target)
            {
                if (d->target->sw_provided)
                    continue;
            }
            else
            {
                auto i = getChildren().find(d->getPackage());
                if (i != getChildren().end())
                {
                    if (i->second.begin()->second->sw_provided)
                        continue;
                }
            }

            nlohmann::json jd;
            jd["path"] = d->getPackage().ppath.toString();
            jd["range"] = d->getPackage().range.toString();
            j["dependencies"].push_back(jd);
        }

        auto s = j.dump();
        m[pkg] = std::make_unique<JsonPackageDescription>(s);
    }
    return m;
}

path Build::getTestDir() const
{
    return BinaryDir / "test" / getSettings().getConfig();
}

void Build::addTest(Test &cb, const String &name)
{
    auto dir = getTestDir() / name;
    fs::remove_all(dir); // also makea condition here

    auto &c = *cb.c;
    c.name = "test: [" + name + "]";
    c.always = true;
    c.working_directory = dir;
    c.addPathDirectory(BinaryDir / getSettings().getConfig());
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
    auto c = tgt.addCommand();
    c << cmd::prog(tgt);
    Test t(c);
    addTest(t, name);
    return t;
}

Test Build::addTest()
{
    return addTest("test." + std::to_string(tests.size() + 1));
}

Test Build::addTest(const String &name)
{
    Test cb(swctx, swctx.getServiceFileStorage());
    addTest(cb, name);
    return cb;
}

}
