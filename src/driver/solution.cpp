// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <solution.h>

#include "solution_build.h"
#include "file_storage.h"
#include "functions.h"
#include "generator/generator.h"
#include "inserts.h"
#include "module.h"
#include "program.h"
#include "resolver.h"
#include "run.h"
#include "frontend/cppan/yaml.h"
#include <target/native.h>

#include <database.h>
#include <execution_plan.h>
#include <hash.h>
#include <settings.h>
#include <storage.h>

#include <sw/driver/sw_abi_version.h>

#include <boost/algorithm/string.hpp>
#include <boost/range/combine.hpp>
#include <boost/thread/lock_types.hpp>
#include <primitives/context.h>
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
DECLARE_STATIC_LOGGER(logger, "solution");

static cl::opt<bool> print_graph("print-graph", cl::desc("Print file with build graph"));
extern cl::opt<bool> dry_run;
cl::opt<int> skip_errors("k", cl::desc("Skip errors"));
static cl::opt<bool> time_trace("time-trace", cl::desc("Record chrome time trace events"));

std::map<sw::PackagePath, sw::Version> gUserSelectedPackages;

// TODO: add '#pragma sw driver ...'

namespace sw
{

path build_configs(const std::unordered_set<ExtendedPackageData> &pkgs);
void sw_check_abi_version(int v);

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
    if (!pkgs.empty() && pkgs.find(t.pkg) == pkgs.end())
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

void SolutionSettings::init()
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
            c.program = "xcrun";
            c.args.push_back("--sdk");
            c.args.push_back(sdktype);
            c.args.push_back("--show-sdk-path");
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

String SolutionSettings::getConfig(const TargetBase *t, bool use_short_config) const
{
    // TODO: add get real config, lengthy and with all info

    String c;

    addConfigElement(c, toString(TargetOS.Type));
    if (TargetOS.Type == OSType::Android)
        addConfigElement(c, Native.SDK.Version.string());
    addConfigElement(c, toString(TargetOS.Arch));
    if (TargetOS.Arch == ArchType::arm || TargetOS.Arch == ArchType::aarch64)
        addConfigElement(c, toString(TargetOS.SubArch)); // concat with previous?
    boost::to_lower(c);

    //addConfigElement(c, Native.getConfig());
    addConfigElement(c, toString(Native.CompilerType));
    auto i = t->getSolution()->extensions.find(".cpp");
    if (i == t->getSolution()->extensions.end())
        throw std::logic_error("no cpp compiler");
    addConfigElement(c, i->second.version.toString(2));
    addConfigElement(c, toString(Native.LibrariesType));
    if (TargetOS.Type == OSType::Windows && Native.MT)
        addConfigElement(c, "mt");
    boost::to_lower(c);
    addConfigElement(c, toString(Native.ConfigurationType));

    return hashConfig(c, use_short_config);
}

String SolutionSettings::getTargetTriplet() const
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

Solution::Solution()
    : base_ptr(*this)
{
    checker.solution = this;

    // canonical makes disk letter uppercase on windows
    SourceDir = fs::canonical(fs::current_path());
    BinaryDir = SourceDir / SW_BINARY_DIR;
}

Solution::Solution(const Solution &rhs)
    : TargetBase(rhs)
    , HostOS(rhs.HostOS)
    , Settings(rhs.Settings)
    , silent(rhs.silent)
    //, show_output(rhs.show_output) // don't pass to checks
    , base_ptr(rhs.base_ptr)
    //, knownTargets(rhs.knownTargets)
    , source_dirs_by_source(rhs.source_dirs_by_source)
    , fs(rhs.fs)
    , fetch_dir(rhs.fetch_dir)
    , with_testing(rhs.with_testing)
    , ide_solution_name(rhs.ide_solution_name)
    , disable_compiler_lookup(rhs.disable_compiler_lookup)
    , config_file_or_dir(rhs.config_file_or_dir)
    , Variables(rhs.Variables)
    , events(rhs.events)
    , file_storage_local(rhs.file_storage_local)
    , command_storage(rhs.command_storage)
    , prefix_source_dir(rhs.prefix_source_dir)
    , build(rhs.build)
    , is_config_build(rhs.is_config_build)
{
    checker.solution = this;
}

Solution::~Solution()
{
    //if (fs)
        //fs->closeLogs();
    clear();
}

void Solution::clear()
{
    events.clear();
}

bool Solution::isKnownTarget(const PackageId &p) const
{
    return knownTargets.empty() ||
        p.ppath.is_loc() ||
        knownTargets.find(p) != knownTargets.end();
}

Target::TargetMap &Solution::getChildren()
{
    return children;
}

const Target::TargetMap &Solution::getChildren() const
{
    return children;
}

bool Solution::exists(const PackageId &p) const
{
    return children.find(p) != children.end();
}

path Solution::getSourceDir(const PackageId &p) const
{
    return p.getDirSrc2();
}

std::optional<path> Solution::getSourceDir(const Source &s, const Version &v) const
{
    auto s2 = s;
    applyVersionToUrl(s2, v);
    auto i = source_dirs_by_source.find(s2);
    if (i == source_dirs_by_source.end())
        return {};
    return i->second;
}

path Solution::getIdeDir() const
{
    const auto compiler_name = boost::to_lower_copy(toString(Settings.Native.CompilerType));
    return BinaryDir / "sln" / ide_solution_name / compiler_name;
}

path Solution::getExecutionPlansDir() const
{
    return getIdeDir().parent_path() / "explans";
}

path Solution::getExecutionPlanFilename() const
{
    String n;
    for (auto &[pkg, _] : TargetsToBuild)
        n += pkg.toString();
    return getExecutionPlansDir() / (getConfig() + "_" + sha1(n).substr(0, 8) + ".explan");
}

bool Solution::skipTarget(TargetScope Scope) const
{
    if (Scope == TargetScope::Test ||
        Scope == TargetScope::UnitTest
        )
        return !with_testing;
    return false;
}

path Solution::getTestDir() const
{
    return BinaryDir / "test" / getConfig();
}

void Solution::addTest(Test &cb, const String &name)
{
    auto dir = getTestDir() / name;
    fs::remove_all(dir); // also makea condition here

    auto &c = *cb.c;
    c.name = "test: [" + name + "]";
    c.always = true;
    c.working_directory = dir;
    c.addPathDirectory(BinaryDir / getConfig());
    c.out.file = dir / "stdout.txt";
    c.err.file = dir / "stderr.txt";
    tests.insert(cb.c);
}

Test Solution::addTest(const ExecutableTarget &t)
{
    return addTest("test." + std::to_string(tests.size() + 1), t);
}

Test Solution::addTest(const String &name, const ExecutableTarget &tgt)
{
    auto c = tgt.addCommand();
    c << cmd::prog(tgt);
    Test t(c);
    addTest(t, name);
    return t;
}

Test Solution::addTest()
{
    return addTest("test." + std::to_string(tests.size() + 1));
}

Test Solution::addTest(const String &name)
{
    Test cb(*fs);
    addTest(cb, name);
    return cb;
}

path Solution::getChecksDir() const
{
    return getServiceDir() / "checks";
}

void Solution::performChecks()
{
    checker.performChecks(getStorage().storage_dir_cfg / getConfig());
}

Commands Solution::getCommands() const
{
    // calling this in any case to set proper command dependencies
    for (auto &p : children)
    {
        for (auto &c : p.second->getCommands())
            c->maybe_unused = builder::Command::MU_TRUE;
    }

    Commands cmds;
    // FIXME: drop children from here, always build only precisely picked TargetsToBuild
    auto &chldr = TargetsToBuild.empty() ? children : TargetsToBuild;
    //if (TargetsToBuild.empty())
        //LOG_WARN("logger", "empty TargetsToBuild");

    // we also must take TargetsToBuild deps
    /*while (1)
    {
        decltype(TargetsToBuild) deps;
        auto sz = TargetsToBuild.size();
        for (auto &[n, t] : TargetsToBuild)
        {
            auto nt = (NativeExecutedTarget*)t.get();
            for (auto &d : nt->Dependencies)
            {
                if (d->IncludeDirectoriesOnly)
                    continue;
                auto l = d->target.lock();
                if (l)
                    deps.emplace(l->pkg, l);
            }
        }
        TargetsToBuild.insert(deps.begin(), deps.end());
        if (sz == TargetsToBuild.size())
            break;
    }*/

    for (auto &[p, t] : chldr)
    {
        auto c = t->getCommands();
        for (auto &c2 : c)
            c2->maybe_unused &= ~builder::Command::MU_TRUE;
        cmds.insert(c.begin(), c.end());

        // copy output dlls

        auto nt = t->as<NativeExecutedTarget>();
        if (!nt)
            continue;
        if (*nt->HeaderOnly)
            continue;
        if (nt->getSelectedTool() == nt->Librarian.get())
            continue;

        if (nt->isLocal() && Settings.Native.CopySharedLibraries &&
            nt->Scope == TargetScope::Build && nt->NativeTarget::getOutputDir().empty())
        {
            for (auto &l : nt->gatherAllRelatedDependencies())
            {
                auto dt = l->as<NativeExecutedTarget>();
                if (!dt)
                    continue;
                if (dt->isLocal())
                    continue;
                if (dt->HeaderOnly.value())
                    continue;
                if (getSolution()->Settings.Native.LibrariesType != LibraryType::Shared && !dt->isSharedOnly())
                    continue;
                if (dt->getSelectedTool() == dt->Librarian.get())
                    continue;
                auto in = dt->getOutputFile();
                auto o = nt->getOutputDir() / dt->NativeTarget::getOutputDir();
                o /= in.filename();
                if (in == o)
                    continue;

                SW_MAKE_EXECUTE_BUILTIN_COMMAND(copy_cmd, *nt, "sw_copy_file");
                copy_cmd->args.push_back(in.u8string());
                copy_cmd->args.push_back(o.u8string());
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

    return cmds;
}

void Solution::printGraph(const path &p) const
{
    String s;
    s += "digraph G {\n";
    for (auto &[p, t] : getChildren())
    {
        auto nt = t->as<NativeExecutedTarget>();
        if (!nt/* || nt->HeaderOnly.value()*/)
            continue;
        //s += "\"" + pp.toString() + "\";\n";
        for (auto &d : nt->Dependencies)
        {
            // TODO: also print dummy and idir deps
            if (d->target && !d->IncludeDirectoriesOnly)
                s += "\"" + p.toString() + "\"->\"" + d->target->pkg.toString() + "\";\n";
        }
    }
    s += "}";
    write_file(p, s);
}

void Solution::clean() const
{
    auto ep = getExecutionPlan();
    for (auto &c : ep.commands)
        c->clean();
}

void Solution::execute()
{
    prepare();
    ((const Solution *)this)->execute();
}

void Solution::execute() const
{
    auto p = getExecutionPlan();
    execute(p);
}

void Solution::execute(CommandExecutionPlan &p) const
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
            for (const auto &[i, s] : enumerate(b->solutions))
                s.printGraph(d / ("solution." + std::to_string(i + 1) + ".dot"));
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

void Solution::build_and_resolve(int n_runs)
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
    Resolver r;
    r.resolve_dependencies(pkgs, true);
    auto dd = r.getDownloadDependencies();
    if (dd.empty())
        throw SW_RUNTIME_ERROR("Empty download dependencies");

    for (auto &p : dd)
        knownTargets.insert(p);

    // gather packages
    std::unordered_map<PackageVersionGroupNumber, ExtendedPackageData> cfgs2;
    for (auto &[p, gn] : r.getDownloadDependenciesWithGroupNumbers())
        cfgs2[gn] = p;
    std::unordered_set<ExtendedPackageData> cfgs;
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

    auto dll = ::sw::build_configs(cfgs);
    //used_modules.insert(dll);

    Local = false;

    SwapAndRestore sr(NamePrefix, cfgs.begin()->ppath.slice(0, cfgs.begin()->prefix));
    if (cfgs.size() != 1)
        sr.restoreNow(true);

    sw_check_abi_version(getModuleStorage(base_ptr).get(dll).sw_get_module_abi_version());
    getModuleStorage(base_ptr).get(dll).check(*this, checker);
    performChecks();
    // we can use new (clone of this) solution, then copy known targets
    // to allow multiple passes-builds
    getModuleStorage(base_ptr).get(dll).build(*this);

    sr.restoreNow(true);

    auto rd = r.resolved_packages;
    for (auto &[porig, p] : rd)
    {
        for (auto &[n, t] : getChildren())
        {
            if (p == t->pkg && ud[porig])
            {
                ud[porig]->setTarget(*std::static_pointer_cast<NativeTarget>(t).get());
                //t->SourceDir = p.getDirSrc2();
            }
        }
    }

    {
        ud = gatherUnresolvedDependencies();
        UnresolvedPackages pkgs;
        for (auto &[pkg, d] : ud)
            pkgs.insert(pkg);
        r.resolve_dependencies(pkgs);

        if (ud.empty())
            return;
    }

    // we have unloaded deps, load them
    // they are runtime deps either due to local overridden packages
    // or to unregistered deps in sw - probably something wrong or
    // malicious

    build_and_resolve(n_runs + 1);
}

void Solution::prepare()
{
    // checks use this
    //throw SW_RUNTIME_ERROR("unreachable");

    //if (prepared)
        //return;

    // all targets are set stay unchanged from user
    // so, we're ready to some preparation passes

    // resolve all deps first
    build_and_resolve();

    // multipass prepare()
    // if we add targets inside this loop,
    // it will automatically handle this situation
    while (prepareStep())
        ;

    //prepared = true;

    // prevent memory leaks (high mem usage)
    //updateConcurrentContext();
}

bool Solution::prepareStep()
{
    // checks use this
    //throw SW_RUNTIME_ERROR("unreachable");

    std::atomic_bool next_pass = false;

    auto &e = getExecutor();
    Futures<void> fs;
    prepareStep(e, fs, next_pass, nullptr);
    waitAndGet(fs);

    return next_pass;
}

void Solution::prepareStep(Executor &e, Futures<void> &fs, std::atomic_bool &next_pass, const Solution *host) const
{
    for (const auto &[pkg, t] : getChildren())
    {
        fs.push_back(e.push([this, t = std::ref(t), &next_pass, host]
        {
            if (prepareStep(t, host))
                next_pass = true;
        }));
    }
}

bool Solution::prepareStep(const TargetBaseTypePtr &t, const Solution *host) const
{
    // try to run as early as possible
    if (t->mustResolveDeps())
        resolvePass(*t, t->gatherDependencies(), host);

    return t->prepare();
}

void Solution::resolvePass(const Target &t, const DependenciesType &deps, const Solution *host) const
{
    bool select_targets = host;
    if (!host)
        host = this;
    for (auto &d : deps)
    {
        auto h = this;
        if (d->Dummy)
            h = host;
        else if (d->isResolved())
            continue;

        auto i = h->getChildren().find(d->getPackage());
        if (i != h->getChildren().end())
        {
            auto t = std::static_pointer_cast<NativeTarget>(i->second);
            if (t)
                d->setTarget(*t);
            else
                throw SW_RUNTIME_ERROR("bad target cast to NativeTarget during resolve");

            // turn on only needed targets during cc
            if (select_targets)
                host->TargetsToBuild[i->second->pkg] = i->second;
        }
        // we fail in any case here, no matter if dependency were resolved previously
        else
        //if (!d->target.lock())
        {
            // allow dummy scoped tools
            auto i = h->dummy_children.find(d->getPackage());
            if (i != h->dummy_children.end() &&
                i->second->Scope == TargetScope::Tool)
            {
                auto t = std::static_pointer_cast<NativeTarget>(i->second);
                if (t)
                    d->setTarget(*t);
                else
                    throw SW_RUNTIME_ERROR("bad target cast to NativeTarget during resolve");

                // turn on only needed targets during cc
                if (select_targets)
                    host->TargetsToBuild[i->second->pkg] = i->second;
            }
            else
            {
                auto err = "Package: " + t.pkg.toString() + ": Unresolved package on stage 1: " + d->getPackage().toString();
                if (d->target)
                    err += " (but target is set to " + d->target->getPackage().toString() + ")";
                if (auto d = t.pkg.getOverriddenDir(); d)
                {
                    err += ".\nPackage: " + t.pkg.toString() + " is overridden locally. "
                        "This means you have new dependency that is not in db.\n"
                        "Run following command in attempt to fix this issue: "
                        "'sw -d " + normalize_path(d.value()) + " -override-remote-package " +
                        t.pkg.ppath.slice(0, getServiceDatabase().getOverriddenPackage(t.pkg).value().prefix).toString() + "'";
                }
                throw std::logic_error(err);
            }
        }
    }
}

UnresolvedDependenciesType Solution::gatherUnresolvedDependencies(int n_runs) const
{
    UnresolvedDependenciesType deps;
    std::unordered_set<UnresolvedPackage> known;

    for (const auto &p : getChildren())
    {
        auto c = p.second->gatherUnresolvedDependencies();
        if (c.empty())
            continue;

        for (auto &r : known)
            c.erase(r);
        if (c.empty())
            continue;

        std::unordered_set<UnresolvedPackage> known2;
        for (auto &[up, dptr] : c)
        {
            if (auto r = getPackageStore().isPackageResolved(up); r)
            {
                auto i = children.find(r.value());
                if (i != children.end())
                {
                    dptr->setTarget(*std::static_pointer_cast<NativeTarget>(i->second).get());
                    known2.insert(up);
                    continue;
                }
            }

            auto i = getChildren().find(up);
            if (i != getChildren().end())
            {
                dptr->setTarget(*std::static_pointer_cast<NativeTarget>(i->second).get());
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

            LOG_ERROR(logger, p.first.toString() + " unresolved deps on run " << n_runs << ": " + s);
        }
    }
    return deps;
}

Solution::CommandExecutionPlan Solution::getExecutionPlan() const
{
    return getExecutionPlan(getCommands());
}

Solution::CommandExecutionPlan Solution::getExecutionPlan(const Commands &cmds) const
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

void Solution::call_event(TargetBase &t, CallbackType et)
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

const StringSet &Solution::getAvailableFrontendNames()
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

const std::set<FrontendType> &Solution::getAvailableFrontendTypes()
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

const Solution::AvailableFrontends &Solution::getAvailableFrontends()
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

const FilesOrdered &Solution::getAvailableFrontendConfigFilenames()
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

bool Solution::isFrontendConfigFilename(const path &fn)
{
    return !!selectFrontendByFilename(fn);
}

std::optional<FrontendType> Solution::selectFrontendByFilename(const path &fn)
{
    auto i = getAvailableFrontends().right.find(fn.filename());
    if (i == getAvailableFrontends().right.end())
        return {};
    return i->get_left();
}

void Solution::setSettings()
{
    fs = &getFileStorage(getConfig(), file_storage_local);

    for (auto &[_, p] : registered_programs)
        p->fs = fs;

    if (Settings.Native.Librarian)
        Settings.Native.Librarian->fs = fs;
    if (Settings.Native.Linker)
        Settings.Native.Linker->fs = fs;
}

void Solution::findCompiler()
{
    Settings.init();

    if (!disable_compiler_lookup)
        detectCompilers(*this);

    using CompilerVector = std::vector<std::pair<PackageId, CompilerType>>;

    auto activate_one = [this](auto &v)
    {
        auto r = activateLanguage(v.first.ppath);
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
        switch (HostOS.Type)
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
                auto cmd = l->createCommand();
                cmd->args.push_back("-fuse-ld=lld");
                cmd->args.push_back("-target");
                cmd->args.push_back(Settings.getTargetTriplet());
            }
        }
    }

    // lib/link
    auto activate_lib_link_or_throw = [this](const std::vector<std::tuple<PackagePath /* lib */, LinkerType>> &a, const auto &e, bool link = false)
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
        activateLanguage(a);

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
        extensions.erase(".m");
        extensions.erase(".mm");
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
}

bool Solution::canRunTargetExecutables() const
{
    return HostOS.canRunTargetExecutables(Settings.TargetOS);
}

void Solution::prepareForCustomToolchain()
{
    extensions.clear();
    user_defined_languages.clear();
    registered_programs.clear();
    disable_compiler_lookup = true;
}

PackageDescriptionMap Solution::getPackages() const
{
    PackageDescriptionMap m;

    for (auto &[pkg, t] : children)
    {
        // deps
        if (pkg.ppath.isAbsolute())
            continue;

        // do not participate in build
        if (t->Scope != TargetScope::Build)
            continue;

        nlohmann::json j;

        // source, version, path
        save_source(j["source"], t->source);
        j["version"] = pkg.getVersion().toString();
        j["path"] = pkg.ppath.toString();

        auto rd = SourceDir;
        if (!build->fetch_info.sources.empty())
        {
            auto src = t->source; // copy
            checkSourceAndVersion(src, t->pkg.version);
            auto si = build->fetch_info.sources.find(src);
            if (si == build->fetch_info.sources.end())
                throw SW_RUNTIME_ERROR("no such source");
            rd = si->second;
        }
        j["root_dir"] = normalize_path(rd);

        // files
        // we do not use nt->gatherSourceFiles(); as it removes deleted files
        Files files;
        for (auto &f : t->gatherAllFiles())
        {
            if (File(f, *fs).isGeneratedAtAll())
                continue;
            files.insert(f.lexically_normal());
        }

        if (auto nt = t->as<NativeExecutedTarget>())
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

}
