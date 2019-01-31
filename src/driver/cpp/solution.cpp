// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <solution.h>

#include "checks_storage.h"
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

#include <directories.h>
#include <database.h>
#include <hash.h>
#include <settings.h>

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
#include <primitives/sw/settings.h>

#include <boost/dll.hpp>
#include <nlohmann/json.hpp>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "solution");

static cl::opt<bool> print_graph("print-graph", cl::desc("Print file with build graph"));
cl::opt<String> cl_generator("G", cl::desc("Generator"));
cl::alias generator2("g", cl::desc("Alias for -G"), cl::aliasopt(cl_generator));
static cl::opt<bool> do_not_rebuild_config("do-not-rebuild-config", cl::Hidden);
cl::opt<bool> dry_run("n", cl::desc("Dry run"));
cl::opt<int> skip_errors("k", cl::desc("Skip errors"));
static cl::opt<bool> debug_configs("debug-configs", cl::desc("Build configs in debug mode"));
static cl::opt<bool> fetch_sources("fetch", cl::desc("Fetch files in process"));
static cl::opt<bool> time_trace("time-trace", cl::desc("Record chrome time trace events"));

static cl::opt<int> config_jobs("jc", cl::desc("Number of config jobs"));

static cl::list<String> target_os("target-os", cl::CommaSeparated);
static cl::list<String> compiler("compiler", cl::desc("Set compiler"), cl::CommaSeparated);
static cl::list<String> configuration("configuration", cl::desc("Set build configuration"), cl::CommaSeparated);
cl::alias configuration2("config", cl::desc("Alias for -configuration"), cl::aliasopt(configuration));
static cl::list<String> platform("platform", cl::desc("Set build platform"), cl::CommaSeparated);
//static cl::opt<String> arch("arch", cl::desc("Set arch")/*, cl::sub(subcommand_ide)*/);

// simple -static, -shared?
static cl::opt<bool> static_build("static-build", cl::desc("Set static build"));
cl::alias static_build2("static", cl::desc("Alias for -static-build"), cl::aliasopt(static_build));
static cl::opt<bool> shared_build("shared-build", cl::desc("Set shared build"));
cl::alias shared_build2("shared", cl::desc("Alias for -shared-build"), cl::aliasopt(shared_build));

// simple -mt, -md?
static cl::opt<bool> win_mt("win-mt", cl::desc("Set /MT build"));
cl::alias win_mt2("mt", cl::desc("Alias for -win-mt"), cl::aliasopt(win_mt));
static cl::opt<bool> win_md("win-md", cl::desc("Set /MD build"));
cl::alias win_md2("md", cl::desc("Alias for -win-md"), cl::aliasopt(win_md));

extern bool gVerbose;
bool gWithTesting;

void build_self(sw::Solution &s);
void check_self(sw::Checker &c);

namespace sw
{

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
    return shorten_hash(sha1(getProgramName()));
}

static path getImportFilePrefix()
{
    return getUserDirectories().storage_dir_tmp / ("sw_" + getCurrentModuleId());
}

static path getImportDefinitionsFile()
{
    return getImportFilePrefix() += ".def";
}

static path getImportLibraryFile()
{
    return getImportFilePrefix() += ".lib";
}

static path getImportPchFile()
{
    return getImportFilePrefix() += ".cpp";
}

static const std::regex r_header("#pragma sw header on(.*)#pragma sw header off");

static path getPackageHeader(const ExtendedPackageData &p /* resolved pkg */, const UnresolvedPackage &up)
{
    // depends on upkg, not on pkg!
    // because p is constant, but up might differ
    auto h = p.getDirSrc() / "gen" / ("pkg_header_" + shorten_hash(sha1(up.toString())) + ".h");
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
    //if (std::regex_search(f, m, r_header))
    {
        primitives::Context ctx;
        ctx.addLine("#pragma once");
        ctx.addLine();

        primitives::Context prefix;
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

static std::tuple<FilesOrdered, UnresolvedPackages> getFileDependencies(const path &p)
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
            auto pkg = upkg.resolve();
            auto h = getPackageHeader(pkg, upkg);
            auto [headers2,udeps2] = getFileDependencies(h);
            headers.insert(headers.end(), headers2.begin(), headers2.end());
            udeps.insert(udeps2.begin(), udeps2.end());
            headers.push_back(h);
        }
        else if (m1 == "local")
        {
            auto [headers2, udeps2] = getFileDependencies(m[3].str());
            headers.insert(headers.end(), headers2.begin(), headers2.end());
            udeps.insert(udeps2.begin(), udeps2.end());
        }
        else
            udeps.insert(extractFromString(m1));
        f = m.suffix().str();
    }

    return { headers, udeps };
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

String Solution::SettingsX::getConfig(const TargetBase *t, bool use_short_config) const
{
    String c;

    addConfigElement(c, toString(TargetOS.Type));
    addConfigElement(c, toString(TargetOS.Arch));
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
{
    checker.solution = this;
}

Solution::~Solution()
{
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

TargetBaseTypePtr Solution::resolveTarget(const UnresolvedPackage &pkg) const
{
    throw SW_RUNTIME_ERROR("disabled");

    /*if (resolved_targets.find(pkg) == resolved_targets.end())
    {
        for (const auto &[p, t] : getChildren())
        {
            if (pkg.canBe(p))
            {
                resolved_targets[pkg] = t;
            }
        }
    }
    return resolved_targets[pkg];*/
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

static void addImportLibrary(NativeExecutedTarget &t)
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
    write_file_if_different(getImportDefinitionsFile(), defs);

    auto c = t.addCommand();
    c << t.Librarian->file
        << cmd::in(getImportDefinitionsFile(), cmd::Prefix{ "-DEF:" })
        << cmd::out(getImportLibraryFile(), cmd::Prefix{ "-OUT:" })
        ;
    t.LinkLibraries.push_back(getImportLibraryFile());
#endif

    /*auto o = Local;
    Local = false; // this prevents us from putting compiled configs into user bdirs
    IsConfig = true;
    auto &t = addTarget<StaticLibraryTarget>("sw_implib_" + getCurrentModuleId(), "local");
    IsConfig = false;
    Local = o;
    t.AutoDetectOptions = false;
    t += getImportDefinitionsFile();*/
}

path Solution::getChecksDir() const
{
    return getServiceDir() / "checks";
}

void Solution::performChecks()
{
    checker.performChecks(getUserDirectories().storage_dir_cfg / getConfig());
}

Commands Solution::getCommands() const
{
    //checkPrepared();

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

    for (auto &p : chldr)
    {
        auto c = p.second->getCommands();
        for (auto &c2 : c)
            c2->maybe_unused &= ~builder::Command::MU_TRUE;
        cmds.insert(c.begin(), c.end());
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
        c->silent = silent;

    std::atomic_size_t current_command = 1;
    std::atomic_size_t total_commands = 0;
    for (auto &c : p.commands)
    {
        if (!c->outputs.empty())
            total_commands++;
    }

    for (auto &c : p.commands)
    {
        c->total_commands = &total_commands;
        c->current_command = &current_command;
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
    auto ud = gatherUnresolvedDependencies();
    if (ud.empty())
        return;

    // first round
    UnresolvedPackages pkgs;
    for (auto &[pkg, d] : ud)
        pkgs.insert(pkg);

    if (n_runs > 1)
        LOG_ERROR(logger, "You are here for the third time. This is not intended. Failures are imminent.");

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
        LOG_ERROR(logger, "You are here for the second time. This is not intended. Expect failures.");

    //static
    Build b; // cache?
    b.execute_jobs = config_jobs;
    b.Local = false;
    auto dll = b.build_configs(cfgs);
    //used_modules.insert(dll);

    Local = false;

    SwapAndRestore sr(NamePrefix, cfgs.begin()->ppath.slice(0, cfgs.begin()->prefix));
    if (cfgs.size() != 1)
        sr.restoreNow(true);

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

UnresolvedDependenciesType Solution::gatherUnresolvedDependencies() const
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

    throw SW_RUNTIME_ERROR("Cannot create execution plan because of cyclic dependencies");
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

const boost::bimap<FrontendType, path> &Solution::getAvailableFrontends()
{
    static boost::bimap<FrontendType, path> m = []
    {
        boost::bimap<FrontendType, path> m;
        m.insert({ FrontendType::Sw, "sw.cpp" });
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
    fs = &getFileStorage(getConfig());

    for (auto &[_, p] : registered_programs)
        p->fs = fs;

    if (Settings.Native.Librarian)
        Settings.Native.Librarian->fs = fs;
    if (Settings.Native.Linker)
        Settings.Native.Linker->fs = fs;
}

void Solution::findCompiler()
{
    if (!disable_compiler_lookup)
        detectCompilers(*this);

    using CompilerVector = std::vector<std::pair<PackagePath, CompilerType>>;

    auto activate_one = [this](auto &v)
    {
        auto r = activateLanguage(v.first);
        if (r)
            this->Settings.Native.CompilerType = v.second;
        return r;
    };

    auto activate = [&activate_one](const CompilerVector &a)
    {
        return std::any_of(a.begin(), a.end(), [&activate_one](const auto &v)
        {
            return activate_one(v);
        });
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
            if (r)
                LOG_TRACE(logger, "activated " << v.begin()->first.toString() << " successfully");
            else
                LOG_TRACE(logger, "activate " << v.begin()->first.toString() << " failed");
            return r;
        });
    };

    auto activate_or_throw = [&activate](const CompilerVector &a, const auto &e)
    {
        if (!activate(a))
            throw SW_RUNTIME_ERROR(e);
    };

    auto activate_array_or_throw = [&activate_array](const std::vector<CompilerVector> &a, const auto &e)
    {
        if (!activate_array(a))
            throw SW_RUNTIME_ERROR(e);
    };

    auto activate_linker_or_throw = [this](const std::vector<std::tuple<PackagePath /* lib */, PackagePath /* link */, LinkerType>> &a, const auto &e)
    {
        if (!std::any_of(a.begin(), a.end(), [this](const auto &v)
        {
            auto lib = getProgram(std::get<0>(v));
            auto link = getProgram(std::get<1>(v));
            auto r = lib && link;
            if (r)
            {
                this->Settings.Native.Librarian = std::dynamic_pointer_cast<NativeLinker>(lib->clone());
                this->Settings.Native.Linker = std::dynamic_pointer_cast<NativeLinker>(link->clone());
                //this->Settings.Native.LinkerType = std::get<2>(v);
            }
            return r;
        }))
            throw SW_RUNTIME_ERROR(e);
    };

    const CompilerVector msvc =
    {
        {"com.Microsoft.VisualStudio.VC.clpp", CompilerType::MSVC},
        {"com.Microsoft.VisualStudio.VC.cl", CompilerType::MSVC},
        {"com.Microsoft.VisualStudio.VC.ml", CompilerType::MSVC},
        {"com.Microsoft.VisualStudio.VC.rc", CompilerType::MSVC},
    };

    const CompilerVector gnu =
    {
        {"org.gnu.gcc.gpp", CompilerType::GNU},
        {"org.gnu.gcc.gcc", CompilerType::GNU},
        {"org.gnu.gcc.as", CompilerType::GNU},
    };

    const CompilerVector clang =
    {
        { "org.LLVM.clangpp", CompilerType::Clang },
        { "org.LLVM.clang", CompilerType::Clang},
    };

    const CompilerVector clangcl =
    {
        { "org.LLVM.clangcl",CompilerType::ClangCl }
    };

    const CompilerVector other =
    {
        {"com.Microsoft.VisualStudio.Roslyn.csc", CompilerType::MSVC},
        {"org.rust.rustc", CompilerType::MSVC},
        {"org.google.golang.go", CompilerType::MSVC},
        {"org.gnu.gcc.fortran", CompilerType::MSVC},
        {"com.oracle.java.javac", CompilerType::MSVC},
        {"com.JetBrains.kotlin.kotlinc", CompilerType::MSVC},
        {"org.dlang.dmd.dmd", CompilerType::MSVC},
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
    case CompilerType::GNU:
        activate_array_or_throw({ gnu }, "Cannot find gnu toolchain");
        break;
    case CompilerType::UnspecifiedCompiler:
        switch (HostOS.Type)
        {
        case OSType::Windows:
            activate_array_or_throw({ msvc, clang, clangcl, }, "Try to add more compilers");
            break;
        case OSType::Cygwin:
        case OSType::Linux:
            activate_array_or_throw({ gnu, clang, }, "Try to add more compilers");
            break;
        case OSType::Macos:
            activate_array_or_throw({ clang, gnu, }, "Try to add more compilers");
            break;
        }
        break;
    default:
        throw SW_RUNTIME_ERROR("solution.cpp: not implemented");
    }

    if (Settings.TargetOS.Type != OSType::Macos)
    {
        extensions.erase(".m");
        extensions.erase(".mm");
    }

    if (HostOS.is(OSType::Windows))
    {
        activate_linker_or_throw({
            {"com.Microsoft.VisualStudio.VC.lib", "com.Microsoft.VisualStudio.VC.link",LinkerType::MSVC},
            {"org.gnu.binutils.ar", "org.gnu.gcc.ld",LinkerType::GNU},
            {"org.gnu.binutils.ar", "org.LLVM.clang.ld",LinkerType::GNU},
            }, "Try to add more linkers");
    }
    else
    {
        activate_linker_or_throw({
            // base
            {"org.gnu.binutils.ar", "org.gnu.gcc.ld",LinkerType::GNU},
            {"org.gnu.binutils.ar", "org.LLVM.clang.ld",LinkerType::GNU},
            // cygwin alternative, remove?
            {"com.Microsoft.VisualStudio.VC.lib", "com.Microsoft.VisualStudio.VC.link",LinkerType::MSVC},
            }, "Try to add more linkers");
    }

    // more languages
    for (auto &[a, _] : other)
        activateLanguage(a);

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

Build::Build()
{
    HostOS = getHostOS();
    Settings.TargetOS = HostOS; // default

    //languages = getLanguages();
    findCompiler();
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

Solution &Build::addSolution()
{
    return solutions.emplace_back(*this);
}

Solution &Build::addCustomSolution()
{
    auto &s = addSolution();
    s.prepareForCustomToolchain();
    return s;
}

static auto getFilesHash(const Files &files)
{
    String h;
    for (auto &fn : files)
        h += fn.u8string();
    return sha256_short(h);
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

static void addDeps(NativeExecutedTarget &lib, Solution &solution)
{
    lib += solution.getTarget<NativeTarget>("pub.egorpugin.primitives.version");

    auto &drv = solution.getTarget<NativeTarget>("org.sw.sw.client.driver.cpp");
    auto d = lib + drv;
    d->IncludeDirectoriesOnly = true;

    // generated file
    lib += drv.BinaryDir / "options_cl.generated.h";
}

// add Dirs?
static path getDriverIncludeDir(Solution &solution)
{
    return solution.getTarget<NativeTarget>("org.sw.sw.client.driver.cpp").SourceDir / "include";
}

static path getDriverIncludePath(Solution &solution, const path &fn)
{
    return getDriverIncludeDir(solution) / fn;
}

static String getDriverIncludePathString(Solution &solution, const path &fn)
{
    return normalize_path(getDriverIncludeDir(solution) / fn);
}

static path getMainPchFilename()
{
    return "sw/driver/cpp/sw.h";
}

static void write_pch(Solution &solution)
{
    write_file_if_different(getImportPchFile(),
        "#include <" + getDriverIncludePathString(solution, getMainPchFilename()) + ">\n\n" +
        cppan_cpp);
}

path Build::getOutputModuleName(const path &p)
{
    if (solutions.empty())
        addSolution();

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

    // reset before start adding targets
    //getFileStorage().reset();

    if (solutions.empty())
        addSolution();

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

        addImportLibrary(lib);
        lib.AutoDetectOptions = false;
        lib.CPPVersion = CPPLanguageStandard::CPP17;

        lib += fn;
        write_pch(solution);
        PrecompiledHeader pch;
        pch.header = getDriverIncludePathString(solution, getMainPchFilename());
        pch.source = getImportPchFile();
        pch.force_include_pch = true;
        lib.addPrecompiledHeader(pch);

        auto [headers, udeps] = getFileDependencies(fn);

        for (auto &h : headers)
        {
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
            if (auto c = sf->compiler->template as<ClangCompiler>())
            {
                //throw SW_RUNTIME_ERROR("pchs are not implemented for clang");
            }
            else if (auto c = sf->compiler->template as<GNUCompiler>())
            {
                c->ForcedIncludeFiles().push_back(getDriverIncludeDir(solution) / "sw/driver/cpp/sw1.h");
            }
        }

#if defined(CPPAN_OS_WINDOWS)
        lib.Definitions["SW_SUPPORT_API"] = "__declspec(dllimport)";
        lib.Definitions["SW_MANAGER_API"] = "__declspec(dllimport)";
        lib.Definitions["SW_BUILDER_API"] = "__declspec(dllimport)";
        lib.Definitions["SW_DRIVER_CPP_API"] = "__declspec(dllimport)";
        // do not use api name because we use C linkage
        lib.Definitions["SW_PACKAGE_API"] = "extern \"C\" __declspec(dllexport)";
#else
        lib.Definitions["SW_SUPPORT_API="];
        lib.Definitions["SW_MANAGER_API="];
        lib.Definitions["SW_BUILDER_API="];
        lib.Definitions["SW_DRIVER_CPP_API="];
        // do not use api name because we use C linkage
        lib.Definitions["SW_PACKAGE_API"] = "extern \"C\" __attribute__ ((visibility (\"default\")))";
#endif

#if defined(CPPAN_OS_WINDOWS)
        lib.LinkLibraries.insert("Delayimp.lib");
#endif

        if (auto L = lib.Linker->template as<VisualStudioLinker>())
        {
            L->DelayLoadDlls().push_back(IMPORT_LIBRARY);
            //#ifdef CPPAN_DEBUG
            L->GenerateDebugInfo = true;
            //#endif
            L->Force = vs::ForceType::Multiple;
            L->IgnoreWarnings().insert(4006); // warning LNK4006: X already defined in Y; second definition ignored
            L->IgnoreWarnings().insert(4070); // warning LNK4070: /OUT:X.dll directive in .EXP differs from output filename 'Y.dll'; ignoring directive
            L->IgnoreWarnings().insert(4088); // warning LNK4088: image being generated due to /FORCE option; image may not run
        }

        for (auto &d : udeps)
            lib += std::make_shared<Dependency>(d);

        auto i = solution.children.find(lib.pkg);
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

path Build::build_configs(const std::unordered_set<ExtendedPackageData> &pkgs)
{
    if (pkgs.empty())
        return {};

    bool init = false;
    if (solutions.empty())
    {
        addSolution();

        auto &solution = solutions[0];

        solution.Settings.Native.LibrariesType = LibraryType::Static;
        if (debug_configs)
            solution.Settings.Native.ConfigurationType = ConfigurationType::Debug;

        init = true;
    }

    auto &solution = solutions[0];

    Files files;
    std::unordered_map<path, PackageId> output_names;
    for (auto &pkg : pkgs)
    {
        auto p = pkg.getDirSrc2() / "sw.cpp";
        files.insert(p);
        output_names[p] = pkg;
    }
    bool many_files = files.size() > 1;
    auto h = getFilesHash(files);

    auto &lib = createTarget(files);

    SCOPE_EXIT
    {
        solution.children.erase(lib.pkg);
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

    addImportLibrary(lib);
    lib.AutoDetectOptions = false;
    lib.CPPVersion = CPPLanguageStandard::CPP17;

    // separate loop
    for (auto &[fn, pkg] : output_names)
    {
        lib += fn;
        lib[fn].fancy_name = "[" + output_names[fn].toString() + "]/[config]";
        // configs depend on pch, and pch depends on getCurrentModuleId(), so we add name to the file
        // to make sure we have different config .objs for different pchs
        lib[fn].as<NativeSourceFile>()->setOutputFile(lib, fn.u8string() + "." + getCurrentModuleId(), getObjectDir(pkg) / "self");
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
        primitives::CppContext ctx;

        primitives::CppContext build;
        build.beginFunction("void build(Solution &s)");

        primitives::CppContext check;
        check.beginFunction("void check(Checker &c)");

        for (auto &r : pkgs)
        {
            auto fn = r.getDirSrc2() / "sw.cpp";
            auto h = getFilesHash({ fn });
            ctx.addLine("// " + r.toString());
            ctx.addLine("// " + normalize_path(fn));
            if (HostOS.Type != OSType::Windows)
                ctx.addLine("extern \"C\"");
            ctx.addLine("void build_" + h + "(Solution &);");
            if (HostOS.Type != OSType::Windows)
                ctx.addLine("extern \"C\"");
            ctx.addLine("void check_" + h + "(Checker &);");
            ctx.addLine();

            build.addLine("// " + r.toString());
            build.addLine("// " + normalize_path(fn));
            build.addLine("s.NamePrefix = \"" + r.ppath.slice(0, r.prefix).toString() + "\";");
            build.addLine("s.current_module = \"" + r.toString() + "\";");
            build.addLine("s.current_gn = " + std::to_string(r.group_number) + ";");
            build.addLine("build_" + h + "(s);");
            build.addLine();

            auto cfg = read_file(fn);
            if (cfg.find("void check(") != cfg.npos)
            {
                check.addLine("// " + r.toString());
                check.addLine("c.current_gn = " + std::to_string(r.group_number) + ";");
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

        ctx += build;
        ctx += check;

        auto p = many_files_fn = BinaryDir / "self" / ("sw." + h + ".cpp");
        write_file_if_different(p, ctx.getText());
        lib += p;

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
    pch.header = getDriverIncludePathString(solution, getMainPchFilename());
    pch.source = getImportPchFile();
    pch.force_include_pch = true;
    lib.addPrecompiledHeader(pch);

    for (auto &fn : files)
    {
        auto[headers, udeps] = getFileDependencies(fn);
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
            };

            if (auto c = sf->compiler->template as<VisualStudioCompiler>())
            {
                add_defs(c);
                for (auto &h : headers)
                    c->ForcedIncludeFiles().push_back(h);
            }
            else if (auto c = sf->compiler->template as<ClangClCompiler>())
            {
                add_defs(c);
                for (auto &h : headers)
                    c->ForcedIncludeFiles().push_back(h);
            }
            else if (auto c = sf->compiler->template as<ClangCompiler>())
            {
                throw SW_RUNTIME_ERROR("clang compiler is not implemented");

                for (auto &h : headers)
                    c->ForcedIncludeFiles().push_back(h);
            }
            else if (auto c = sf->compiler->template as<GNUCompiler>())
            {
                // we use pch, but cannot add more defs on CL
                // so we create a file with them
                auto hash = getFilesHash({ fn });
                path h;
                if (is_under_root(fn, getDirectories().storage_dir_pkg))
                    h = fn.parent_path().parent_path() / "aux" / ("defs_" + hash + ".h");
                else
                    h = fn.parent_path() / SW_BINARY_DIR / "aux" / ("defs_" + hash + ".h");
                primitives::CppContext ctx;

                ctx.addLine("#define configure configure_" + hash);
                ctx.addLine("#define build build_" + hash);
                ctx.addLine("#define check check_" + hash);

                write_file_if_different(h, ctx.getText());
                c->ForcedIncludeFiles().push_back(h);
                c->ForcedIncludeFiles().push_back(getDriverIncludeDir(solution) / "sw/driver/cpp/sw1.h");

                for (auto &h : headers)
                    c->ForcedIncludeFiles().push_back(h);
            }
        }
        for (auto &d : udeps)
            lib += std::make_shared<Dependency>(d);
    }

	if (many_files)
    if (auto sf = lib[many_files_fn].template as<NativeSourceFile>())
    {
        if (auto c = sf->compiler->template as<ClangCompiler>())
        {
            //throw SW_RUNTIME_ERROR("pchs are not implemented for clang");
        }
        else if (auto c = sf->compiler->template as<GNUCompiler>())
        {
            c->ForcedIncludeFiles().push_back(getDriverIncludeDir(solution) / "sw/driver/cpp/sw1.h");
        }
    }

#if defined(CPPAN_OS_WINDOWS)
    lib.Definitions["SW_SUPPORT_API"] = "__declspec(dllimport)";
    lib.Definitions["SW_MANAGER_API"] = "__declspec(dllimport)";
    lib.Definitions["SW_BUILDER_API"] = "__declspec(dllimport)";
    lib.Definitions["SW_DRIVER_CPP_API"] = "__declspec(dllimport)";
    // do not use api name because we use C linkage
    lib.Definitions["SW_PACKAGE_API"] = "extern \"C\" __declspec(dllexport)";
#else
    lib.Definitions["SW_SUPPORT_API="];
    lib.Definitions["SW_MANAGER_API="];
    lib.Definitions["SW_BUILDER_API="];
    lib.Definitions["SW_DRIVER_CPP_API="];
    // do not use api name because we use C linkage
    lib.Definitions["SW_PACKAGE_API"] = "extern \"C\" __attribute__ ((visibility (\"default\")))";
#endif

#if defined(CPPAN_OS_WINDOWS)
    lib.LinkLibraries.insert("Delayimp.lib");
#endif

    if (auto L = lib.Linker->template as<VisualStudioLinker>())
    {
        L->DelayLoadDlls().push_back(IMPORT_LIBRARY);
        //#ifdef CPPAN_DEBUG
        L->GenerateDebugInfo = true;
        //#endif
        L->Force = vs::ForceType::Multiple;
        L->IgnoreWarnings().insert(4006); // warning LNK4006: X already defined in Y; second definition ignored
        L->IgnoreWarnings().insert(4070); // warning LNK4070: /OUT:X.dll directive in .EXP differs from output filename 'Y.dll'; ignoring directive
    }

    auto i = solution.children.find(lib.pkg);
    if (i == solution.children.end())
        throw std::logic_error("config target not found");
    solution.TargetsToBuild[i->first] = i->second;

    Solution::execute();

    return lib.getOutputFile();
}

const Module &Build::loadModule(const path &p) const
{
    auto fn2 = p;
    if (!fn2.is_absolute())
        fn2 = SourceDir / fn2;

    Build b;
    b.execute_jobs = config_jobs;
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
        Build b;
        b.execute_jobs = config_jobs;
        auto r = b.build_configs_separate({ fn });
        dll = r.begin()->second;
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
        ide_solution_name = fs::canonical(file_or_dir).filename().u8string();
    else
        ide_solution_name = file_or_dir.stem().u8string();
}

void Build::load(const path &fn, bool configless)
{
    if (!cl_generator.empty())
        generator = Generator::create(cl_generator);

    if (configless)
        return load_configless(fn);

    build(fn);

    //fs->save(); // remove?
    //fs->reset();

    if (fetch_sources)
        fetch_dir = BinaryDir / "src";

    auto fe = selectFrontendByFilename(fn);
    switch (fe.value())
    {
    case FrontendType::Sw:
        load_dll(dll);
        break;
    case FrontendType::Cppan:
        cppan_load();
        break;
    }
}

static Solution::CommandExecutionPlan load(const path &fn, const Solution &s)
{
    primitives::BinaryContext ctx;
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

    auto add_command = [&commands, &s, &read_string](size_t id, uint8_t type)
    {
        auto it = commands.find(id);
        if (it == commands.end())
        {
            std::shared_ptr<builder::Command> c;
            switch (type)
            {
            case 1:
            {
                auto c2 = std::make_shared<driver::cpp::VSCommand>();
                //c2->file.fs = s.fs;
                c = c2;
                //c2->file.file = read_string();
            }
                break;
            case 2:
            {
                auto c2 = std::make_shared<driver::cpp::GNUCommand>();
                //c2->file.fs = s.fs;
                c = c2;
                //c2->file.file = read_string();
                c2->deps_file = read_string();
            }
                break;
            case 3:
            {
                auto c2 = std::make_shared<driver::cpp::ExecuteBuiltinCommand>();
                c = c2;
            }
                break;
            default:
                c = std::make_shared<builder::Command>();
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
    primitives::BinaryContext ctx;

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
        ctx.write(c.get());

        uint8_t type = 0;
        if (auto c2 = c->as<driver::cpp::VSCommand>(); c2)
        {
            type = 1;
            ctx.write(type);
            //print_string(c2->file.file.u8string());
        }
        else if (auto c2 = c->as<driver::cpp::GNUCommand>(); c2)
        {
            type = 2;
            ctx.write(type);
            //print_string(c2->file.file.u8string());
            print_string(c2->deps_file.u8string());
        }
        else if (auto c2 = c->as<driver::cpp::ExecuteBuiltinCommand>(); c2)
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

bool Build::execute()
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

                auto p = ::sw::load(fn, s);
                s.execute(p);
                return true;
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
        return true;
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

    return true;
}

void Build::load_configless(const path &file_or_dir)
{
    setupSolutionName(file_or_dir);

    load_dll({}, false);

    bool dir = fs::is_directory(config_file_or_dir);

    auto &s = solutions[0];
    auto &exe = s.addExecutable(ide_solution_name);
    bool read_deps_from_comments = false;
    if (!dir)
    {
        exe += file_or_dir;

        // read deps from comments
        // read_deps_from_comments = true;
    }

    if (!read_deps_from_comments)
    {
        for (auto &[p, d] : getPackageStore().resolved_packages)
        {
            if (d.installed)
                exe += std::make_shared<Dependency>(p.toString());
        }
    }
}

void Build::build_and_run(const path &fn)
{
    load(fn);
    prepare();
    if (getGenerator())
        return generateBuildSystem();
    Solution::execute();
}

void Build::generateBuildSystem()
{
    if (!getGenerator())
        return;

    getCommands();

    fs::remove_all(getExecutionPlansDir());
    getGenerator()->generate(*this);
}

void Build::build_package(const String &s)
{
    //auto [pkg,pkgs] = resolve_dependency(s);
    auto pkg = extractFromString(s);

    // add default sln
    auto &sln = addSolution();

    // add known pkgs before pkg.resolve(), because otherwise it does not give us dl deps
    for (auto &p : resolveAllDependencies({ pkg }))
        sln.knownTargets.insert(p);

    auto r = pkg.resolve();
    sln.Local = false;
    sln.NamePrefix = pkg.ppath.slice(0, r.prefix);
    build_and_run(r.getDirSrc2() / "sw.cpp");
}

void Build::run_package(const String &s)
{
    build_package(s);

    auto p = solutions[0].getTargetPtr(extractFromString(s).resolve())->as<NativeExecutedTarget>();
    if (!p || p->getType() != TargetType::NativeExecutable)
        throw SW_RUNTIME_ERROR("Unsupported package type");

    auto cb = p->addCommand();

    cb.c->program = p->getOutputFile();
    cb.c->working_directory = p->pkg.getDirObjWdir();
    fs::create_directories(cb.c->working_directory);
    p->setupCommandForRun(*cb.c);
    /*if (cb.c->create_new_console)
    {
        cb.c->inherit = true;
        cb.c->in.inherit = true;
    }
    else*/
        cb.c->detached = true;

    run(p->pkg, *cb.c);
}

static bool hasUserProvidedInformation()
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
        ;
}

void Build::load_dll(const path &dll, bool usedll)
{
    if (gWithTesting)
        with_testing = true;

    // explicit presets
#ifdef _WIN32
    Settings.Native.CompilerType = CompilerType::MSVC;
#elif __APPLE__
    Settings.Native.CompilerType = CompilerType::Clang; // switch to apple clang?
#else
    Settings.Native.CompilerType = CompilerType::GNU;
#endif

    // configure may change defaults, so we must care below
    if (usedll)
        getModuleStorage(base_ptr).get(dll).configure(*this);

    if (solutions.empty())
    {
        if (hasUserProvidedInformation())
        {
            // add basic solution
            addSolution();

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
        }
        else if (auto g = getGenerator(); g)
        {
            g->createSolutions(*this);
        }
    }

    // one more time, if generator did not add solution or whatever
    if (solutions.empty())
        addSolution();

    if (auto g = getGenerator(); g)
    {
        LOG_INFO(logger, "Generating " << toString(g->type) << " project with " << solutions.size() << " configurations:");
        for (auto &s : solutions)
            LOG_INFO(logger, s.getConfig());
    }

    // add cc if needed
    getHostSolution();

    // detect and eliminate solution clones

    // apply config settings
    for (auto &s : solutions)
        s.findCompiler();

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

PackageDescriptionMap Build::getPackages() const
{
    PackageDescriptionMap m;
    if (solutions.empty())
        return m;

    auto &s = *solutions.begin();
    for (auto &[pkg, t] : s.children)
    {
        if (t->Scope != TargetScope::Build)
            continue;

        nlohmann::json j;

        // source, version, path
        save_source(j["source"], t->source);
        j["version"] = pkg.getVersion().toString();
        j["path"] = pkg.ppath.toString();

        auto rd = s.SourceDir;
        if (!fetch_info.sources.empty())
        {
            auto src = t->source; // copy
            checkSourceAndVersion(src, t->pkg.version);
            auto si = fetch_info.sources.find(src);
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
        // 'from' field is calculated relative to fetch/sln dir
        auto files_map1 = primitives::pack::prepare_files(files, rd.lexically_normal());
        // but 'to' field is calculated based on target's own view
        /*auto files_map2 = primitives::pack::prepare_files(files, t->SourceDir.lexically_normal());
        if (files_map1.size() != files_map2.size())
        {
            auto fm2 = files_map2;
            for (auto &[f, _] : files_map1)
                files_map2.erase(f);
            for (auto &[f, _] : fm2)
                files_map1.erase(f);
            String s;
            if (!files_map1.empty())
            {
                s += "from first map: ";
                for (auto &[f, _] : files_map1)
                    s += normalize_path(f) + ", ";
            }
            if (!files_map2.empty())
            {
                s += "from second map: ";
                for (auto &[f, _] : files_map2)
                    s += normalize_path(f) + ", ";
            }
            throw SW_RUNTIME_ERROR("Target: " + pkg.toString() + " Maps are not the same: " + s);
        }
        for (const auto &tup : boost::combine(files_map1, files_map2))
        {
            std::pair<path, path> f1, f2;
            boost::tie(f1, f2) = tup;

            nlohmann::json jf;
            jf["from"] = normalize_path(f1.first);
            jf["to"] = normalize_path(f2.second);
            j["files"].push_back(jf);
        }*/
        for (const auto &[f1, f2] : files_map1)
        {
            nlohmann::json jf;
            jf["from"] = normalize_path(f1);
            jf["to"] = normalize_path(f2);
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
