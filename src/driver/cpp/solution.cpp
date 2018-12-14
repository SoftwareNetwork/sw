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

#include <directories.h>
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

static cl::opt<bool> print_commands("print-commands", cl::desc("Print file with build commands"));
static cl::opt<bool> print_comp_db("print-compilation-database", cl::desc("Print file with build commands in compilation db format"));
cl::opt<String> generator("G", cl::desc("Generator"));
cl::alias generator2("g", cl::desc("Alias for -G"), cl::aliasopt(generator));
static cl::opt<bool> do_not_rebuild_config("do-not-rebuild-config", cl::Hidden);
cl::opt<bool> dry_run("n", cl::desc("Dry run"));
static cl::opt<bool> debug_configs("debug-configs", cl::desc("Build configs in debug mode"));

static cl::opt<String> target_os("target-os");
static cl::opt<String> compiler("compiler", cl::desc("Set compiler")/*, cl::sub(subcommand_ide)*/);
static cl::opt<String> configuration("configuration", cl::desc("Set build configuration")/*, cl::sub(subcommand_ide)*/);
static cl::opt<String> platform("platform", cl::desc("Set build platform")/*, cl::sub(subcommand_ide)*/);
//static cl::opt<String> arch("arch", cl::desc("Set arch")/*, cl::sub(subcommand_ide)*/);
static cl::opt<bool> static_build("static-build", cl::desc("Set static build")/*, cl::sub(subcommand_ide)*/);
static cl::opt<bool> shared_build("shared-build", cl::desc("Set shared build")/*, cl::sub(subcommand_ide)*/);

extern bool gVerbose;
bool gWithTesting;

void build_self(sw::Solution &s);
void check_self(sw::Checker &c);

namespace sw
{

path getImportFilePrefix()
{
    return getUserDirectories().storage_dir_tmp / ("cppan_" + getCurrentModuleNameHash());
}

path getImportDefinitionsFile()
{
    return getImportFilePrefix() += ".def";
}

path getImportPchFile()
{
    return getImportFilePrefix() += ".cpp";
}

path getPackageHeader(const ExtendedPackageData &p)
{
    auto h = p.getDirSrc() / "gen" / "pkg_header.h";
    //if (fs::exists(h))
        //return h;
    auto cfg = p.getDirSrc2() / "sw.cpp";
    auto f = read_file(cfg);
    static const std::regex r_header("#pragma sw header on(.*)#pragma sw header off");
    std::smatch m;
    // replace with while?
    const char on[] = "#pragma sw header on";
    auto pos = f.find(on);
    if (pos == f.npos)
        throw SW_RUNTIME_EXCEPTION("No header for package: " + p.toString());
    f = f.substr(pos + sizeof(on));
    pos = f.find("#pragma sw header off");
    if (pos == f.npos)
        throw SW_RUNTIME_EXCEPTION("No end in header for package: " + p.toString());
    f = f.substr(0, pos);
    //if (std::regex_search(f, m, r_header))
    {
        primitives::Context ctx;
        ctx.addLine("#pragma once");
        ctx.addLine();

        primitives::Context prefix;
        prefix.addLine("#define THIS_PREFIX \"" + p.ppath.slice(0, p.prefix).toString() + "\"");
        prefix.addLine("#define THIS_RELATIVE_PACKAGE_PATH \"" + p.ppath.slice(p.prefix).toString() + "\"");
        prefix.addLine("#define THIS_PACKAGE_PATH THIS_PREFIX \".\" THIS_RELATIVE_PACKAGE_PATH");
        prefix.addLine("#define THIS_VERSION \"" + p.version.toString() + "\"");
        prefix.addLine("#define THIS_VERSION_DEPENDENCY \"" + p.version.toString() + "\"_dep");
        prefix.addLine("#define THIS_PACKAGE THIS_PACKAGE_PATH \"-\" THIS_VERSION");
        prefix.addLine("#define THIS_PACKAGE_DEPENDENCY THIS_PACKAGE_PATH \"-\" THIS_VERSION_DEPENDENCY");
        prefix.addLine();

        auto ins_pre = "#pragma sw header insert prefix";
        if (f.find(ins_pre) != f.npos)
            boost::replace_all(f, ins_pre, prefix.getText());
        else
            ctx += prefix;

        ctx.addLine(f);
        ctx.addLine();

        ctx.addLine("#undef THIS_PREFIX");
        ctx.addLine("#undef THIS_RELATIVE_PACKAGE_PATH");
        ctx.addLine("#undef THIS_PACKAGE_PATH");
        ctx.addLine("#undef THIS_VERSION");
        ctx.addLine("#undef THIS_VERSION_DEPENDENCY");
        ctx.addLine("#undef THIS_PACKAGE");
        ctx.addLine("#undef THIS_PACKAGE_DEPENDENCY");
        ctx.addLine();

        write_file_if_different(h, ctx.getText());
    }
    return h;
}

std::tuple<FilesOrdered, UnresolvedPackages> getFileDependencies(const path &p)
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
            auto pkg = extractFromString(m[3].str()).resolve();
            auto h = getPackageHeader(pkg);
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

Solution::Solution()
    : base_ptr(*this)
{
    Checks.solution = this;

    SourceDir = fs::absolute(fs::current_path());
    BinaryDir = SourceDir / ".sw";
}

Solution::Solution(const Solution &rhs)
    : TargetBase(rhs)
    //, checksStorage(rhs.checksStorage)
    , silent(rhs.silent)
    , base_ptr(rhs.base_ptr)
    //, knownTargets(rhs.knownTargets)
    , source_dirs_by_source(rhs.source_dirs_by_source)
    , fs(rhs.fs)
    , fetch_dir(rhs.fetch_dir)
    , with_testing(rhs.with_testing)
    , ide_solution_name(rhs.ide_solution_name)
    , config_file_or_dir(rhs.config_file_or_dir)
    , events(rhs.events)
{
    Checks.solution = this;
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

optional<path> Solution::getSourceDir(const Source &s, const Version &v) const
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

void Solution::addTest(const ExecutableTarget &t)
{
    addTest("test: [test." + std::to_string(tests.size() + 1) + "]", t);
}

void Solution::addTest(const String &name, const ExecutableTarget &t)
{
    auto c = t.addCommand();
    c << cmd::prog(t);
    c << cmd::wdir(t.getOutputFile().parent_path());
    tests.insert(c.c);
    c.c->name = name;
    c.c->always = true;
}

driver::cpp::CommandBuilder Solution::addTest()
{
    return addTest("test: [test." + std::to_string(tests.size() + 1) + "]");
}

driver::cpp::CommandBuilder Solution::addTest(const String &name)
{
    driver::cpp::CommandBuilder c(*fs);
    tests.insert(c.c);
    c.c->name = name;
    c.c->always = true;
    return c;
}

StaticLibraryTarget &Solution::getImportLibrary()
{
#if defined(CPPAN_OS_WINDOWS)
    HMODULE lib = (HMODULE)primitives::getModuleForSymbol();
    PIMAGE_NT_HEADERS header = (PIMAGE_NT_HEADERS)((BYTE *)lib + ((PIMAGE_DOS_HEADER)lib)->e_lfanew);
    PIMAGE_EXPORT_DIRECTORY exports = (PIMAGE_EXPORT_DIRECTORY)((BYTE *)lib + header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
    assert(exports->AddressOfNames && "No exports found");
    int* names = (int*)((uint64_t)lib + exports->AddressOfNames);
    String defs;
    defs += "LIBRARY " IMPORT_LIBRARY "\n";
    //defs += "LIBRARY " + GetCurrentModuleName() + "\n";
    defs += "EXPORTS\n";
    for (DWORD i = 0; i < exports->NumberOfNames; i++)
    {
        const char *n = (const char *)lib + names[i];
        defs += "    "s + n + "\n";
    }
    write_file_if_different(getImportDefinitionsFile(), defs);
#endif

    auto o = Local;
    Local = false; // this prevents us from putting compiled configs into user bdirs
    IsConfig = true;
    auto &t = addTarget<StaticLibraryTarget>("cppan_implib_" + getCurrentModuleNameHash(), "local");
    //t.init2();
    IsConfig = false;
    Local = o;
    t.AutoDetectOptions = false;
    t += getImportDefinitionsFile();
    return t;
}

path Solution::getChecksFilename() const
{
    return getUserDirectories().storage_dir_cfg / getConfig() / "checks.txt";
}

void Solution::loadChecks()
{
    checksStorage.load(getChecksFilename());

    // add common checks
    for (auto &s : Checks.sets)
    {
        s.second.checkSourceRuns("WORDS_BIGENDIAN", R"(
int IsBigEndian()
{
    volatile int i=1;
    return ! *((char *)&i);
}
int main()
{
    return IsBigEndian();
}
)");
    }
}

void Solution::saveChecks() const
{
    checksStorage.save(getChecksFilename());
}

void Solution::performChecks()
{
    loadChecks();

    auto set_alternatives = [this](auto &c)
    {
        for (auto &p : c->Prefixes)
            checksStorage.checks[p + c->Definition] = c->Value;
        for (auto &d : c->Definitions)
        {
            checksStorage.checks[d] = c->Value;
            for (auto &p : c->Prefixes)
                checksStorage.checks[p + d] = c->Value;
        }
    };

    std::unordered_set<std::shared_ptr<Check>> checks;
    // prepare
    // TODO: pass current solution to test in different configs
    for (auto &[d, c] : Checks.checks)
    {
        if (c->isChecked())
        {
            //set_alternatives(c);
            continue;
        }
        c->updateDependencies();
        checks.insert(c);
    }
    if (checks.empty())
        return;
    auto ep = ExecutionPlan<Check>::createExecutionPlan(checks);
    if (checks.empty())
    {
        // we reset known targets to prevent wrong children creation as dummy targets
        //auto kt = std::move(knownTargets);

        //Executor e(1);
        auto &e = getExecutor();
        ep.execute(e);

        //knownTargets = std::move(kt);

        // remove tmp dir
        error_code ec;
        fs::remove_all(getChecksDir(), ec);

        for (auto &[d, c] : Checks.checks)
        {
            checksStorage.checks[c->Definition] = c->Value;
            set_alternatives(c);
        }

        saveChecks();

        return;
    }

    // error!

    // print our deps graph
    String s;
    s += "digraph G {\n";
    for (auto &c : checks)
    {
        for (auto &d : c->dependencies)
        {
            if (checks.find(std::static_pointer_cast<Check>(d)) == checks.end())
                continue;
            s += c->Definition + "->" + std::static_pointer_cast<Check>(d)->Definition + ";";
        }
    }
    s += "}";

    auto d = getServiceDir();
    write_file(d / "cyclic_deps_checks.dot", s);

    throw SW_RUNTIME_EXCEPTION("Cannot create execution plan because of cyclic dependencies");
}

void Solution::copyChecksFrom(const Solution &s)
{
    Checks = s.Checks;
    Checks.solution = this;
    for (auto &[s, cs] : Checks.sets)
    {
        cs.checker = &Checks;
        for (auto &[d, c] : cs.checks)
            c->checker = &Checks;
    }
    for (auto &[d, c] : Checks.checks)
        c->checker = &Checks;
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
    auto &chldr = TargetsToBuild.empty() ? children : TargetsToBuild;

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
        auto nt = (NativeExecutedTarget*)t.get();
        if (nt->HeaderOnly && nt->HeaderOnly.value())
            continue;
        //s += "\"" + pp.toString() + "\";\n";
        for (auto &d : nt->Dependencies)
        {
            if (!d->IncludeDirectoriesOnly)
                s += "\"" + p.target_name + "\"->\"" + d->target.lock()->pkg.target_name + "\";\n";
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

void Solution::execute(ExecutionPlan<builder::Command> &p) const
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

    auto print_commands = [](const auto &ep, const path &p)
    {
        auto should_print = [](auto &o)
        {
            if (o.find("showIncludes") != o.npos)
                return false;
            return true;
        };

        auto program_name = [](auto n)
        {
            return "CPPAN_PROGRAM_" + std::to_string(n);
        };

        String s;

        // gather programs
        std::unordered_map<path, size_t> programs;
        for (auto &c : ep.commands)
        {
            auto n = programs.size() + 1;
            if (programs.find(c->getProgram()) == programs.end())
                programs[c->getProgram()] = n;
        }

        // print programs
        for (auto &[k, v] : programs)
            s += "set " + program_name(v) + "=\"" + normalize_path(k) + "\"\n";
        s += "\n";

        // print commands
        for (auto &c : ep.commands)
        {
            std::stringstream stream;
            stream << std::hex << c->getHash();
            std::string result(stream.str());

            s += "@rem " + c->getName() + ", hash = 0x" + result + "\n";
            if (!c->needsResponseFile())
            {
                s += "%" + program_name(programs[c->getProgram()]) + "% ";
                for (auto &a : c->args)
                {
                    if (should_print(a))
                        s += "\"" + a + "\" ";
                }
                s.resize(s.size() - 1);
            }
            else
            {
                s += "@echo. 2> response.rsp\n";
                for (auto &a : c->args)
                {
                    if (should_print(a))
                        s += "@echo \"" + a + "\" >> response.rsp\n";
                }
                s += "%" + program_name(programs[c->getProgram()]) + "% @response.rsp";
            }
            s += "\n\n";
        }
        write_file(p, s);
    };

    auto print_commands_raw = [](const auto &ep, const path &p)
    {
        String s;

        // gather programs
        std::unordered_map<path, size_t> programs;
        for (auto &c : ep.commands)
        {
            s += c->program.u8string() + " ";
            for (auto &a : c->args)
                s += a + " ";
            s.resize(s.size() - 1);
            s += "\n\n";
        }

        write_file(p, s);
    };

    auto print_numbers = [](const auto &ep, const path &p)
    {
        String s;

        auto strings = ep.gatherStrings();
        Strings explain;
        explain.resize(strings.size());

        auto print_string = [&strings, &explain, &s](const String &in)
        {
            auto n = strings[in];
            s += std::to_string(n) + " ";
            explain[n - 1] = in;
        };

        for (auto &c : ep.commands)
        {
            print_string(c->program.u8string());
            print_string(c->working_directory.u8string());
            for (auto &a : c->args)
                print_string(a);
            s.resize(s.size() - 1);
            s += "\n";
        }

        String t;
        for (auto &e : explain)
            t += e + "\n";
        if (!s.empty())
            t += "\n";

        write_file(p, t + s);
    };

    auto print_comp_db = [this](const ExecutionPlan<builder::Command> &ep, const path &p)
    {
        auto b = dynamic_cast<const Build*>(this);
        if (!b || b->solutions.empty())
            return;
        static std::set<String> exts{
            ".c", ".cpp", ".cxx", ".c++", ".cc", ".CPP", ".C++", ".CXX", ".C", ".CC"
        };
        nlohmann::json j;
        for (auto &[p, t] : b->solutions[0].children)
        {
            if (!t->isLocal())
                continue;
            for (auto &c : t->getCommands())
            {
                if (c->inputs.empty())
                    continue;
                if (c->working_directory.empty())
                    continue;
                if (c->inputs.size() > 1)
                    continue;
                if (exts.find(c->inputs.begin()->extension().string()) == exts.end())
                    continue;
                nlohmann::json j2;
                j2["directory"] = normalize_path(c->working_directory);
                j2["file"] = normalize_path(*c->inputs.begin());
                j2["arguments"].push_back(normalize_path(c->program));
                for (auto &a : c->args)
                    j2["arguments"].push_back(a);
                j.push_back(j2);
            }
        }
        write_file(p, j.dump(2));
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
    if (::print_commands && !silent) // && !b console mode
    {
        auto d = getServiceDir();

        //message_box(d.string());
        print_graph(p, d / "build.dot");
        printGraph(d / "solution.dot");
        print_commands(p, d / "commands.bat");
        print_commands_raw(p, d / "commands_raw.bat");
        print_numbers(p, d / "numbers.txt");
        print_comp_db(p, d/ "compile_commands.json");
    }

    if (::print_comp_db && !::print_commands && !silent)
    {
        print_comp_db(p, getServiceDir() / "compile_commands.json");
    }

    ScopedTime t;

    //Executor e(1);
    auto &e = getExecutor();

    if (!dry_run)
    {
        p.execute(e);
        if (!silent)
            LOG_INFO(logger, "Build time: " << t.getTimeFloat() << " s.");
    }
}

void Solution::build_and_resolve()
{
    auto ud = gatherUnresolvedDependencies();
    if (ud.empty())
        return;

    // first round
    UnresolvedPackages pkgs;
    for (auto &[pkg, d] : ud)
        pkgs.insert(pkg);

    // resolve only deps needed
    Resolver r;
    r.resolve_dependencies(pkgs, true);
    auto dd = r.getDownloadDependencies();
    if (dd.empty())
        throw SW_RUNTIME_EXCEPTION("Empty download dependencies");

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

    Build b;
    b.Local = false;
    auto dll = b.build_configs(cfgs);
    //used_modules.insert(dll);

    Local = false;

    SwapAndRestore sr(NamePrefix, cfgs.begin()->ppath.slice(0, cfgs.begin()->prefix));
    if (cfgs.size() != 1)
        sr.restoreNow(true);

    getModuleStorage(base_ptr).get(dll).check(*this, Checks);
    performChecks();
    // we can use new (clone of this) solution, then copy known targets
    // to allow multiple passes-builds
    getModuleStorage(base_ptr).get(dll).build(*this);

    sr.restoreNow(true);

    int retries = 0;
    if (retries++ > 10)
    {
        String s = "Too many attempts on resolving packages, probably something wrong. Unresolved dependencies (" +
            std::to_string(ud.size()) + ") are: ";
        for (auto &p : ud)
            s += p.first.toString() + ", ";
        s.resize(s.size() - 2);
        throw std::logic_error(s);
    }

    auto rd = r.resolved_packages;
    for (auto &[porig, p] : rd)
    {
        for (auto &[n, t] : getChildren())
        {
            if (p == t->pkg && ud[porig])
            {
                ud[porig]->target = std::static_pointer_cast<NativeTarget>(t);
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

    build_and_resolve();
}

void Solution::prepare()
{
    if (prepared)
        return;

    // all targets are set stay unchanged from user
    // so, we're ready to some preparation passes

    // resolve all deps first
    build_and_resolve();

    // multipass prepare()
    // if we add targets inside this loop,
    // it will automatically handle this situation
    auto &e = getExecutor();
    for (std::atomic_bool next_pass = true; next_pass;)
    {
        next_pass = false;
        std::vector<Future<void>> fs;
        for (auto &[p, t] : getChildren())
        {
            fs.push_back(e.push([t = t, &next_pass]
            {
                auto np = t->prepare();
                if (!next_pass)
                    next_pass = np;
            }, getChildren().size()));
        }
        waitAndGet(fs);
    }

    prepared = true;
}

UnresolvedDependenciesType Solution::gatherUnresolvedDependencies() const
{
    UnresolvedDependenciesType deps;

    for (const auto &p : getChildren())
    {
        auto c = p.second->gatherUnresolvedDependencies();
        std::unordered_set<UnresolvedPackage> rm;
        for (auto &[up, dptr] : c)
        {
            if (auto r = getPackageStore().isPackageResolved(up); r)
            {
                auto i = children.find(r.value());
                if (i != children.end())
                {
                    dptr->target = std::static_pointer_cast<NativeTarget>(i->second);
                    rm.insert(up);
                    continue;
                }
            }
            for (const auto &[p,t] : getChildren())
            {
                if (up.canBe(p))
                {
                    dptr->target = std::static_pointer_cast<NativeTarget>(t);
                    rm.insert(up);
                    break;
                }
            }
        }
        for (auto &r : rm)
            c.erase(r);
        deps.insert(c.begin(), c.end());
    }
    return deps;
}

void Solution::checkPrepared() const
{
    if (!prepared)
        throw SW_RUNTIME_EXCEPTION("Prepare solution before executing");
}

ExecutionPlan<builder::Command> Solution::getExecutionPlan() const
{
    auto cmds = getCommands();
    return getExecutionPlan(cmds);
}

ExecutionPlan<builder::Command> Solution::getExecutionPlan(Commands &cmds) const
{
    auto ep = ExecutionPlan<builder::Command>::createExecutionPlan(cmds);
    if (cmds.empty())
        return ep;

    // error!

    // print our deps graph
    String s;
    s += "digraph G {\n";
    for (auto &c : cmds)
    {
        for (auto &d : c->dependencies)
        {
            if (cmds.find(d) == cmds.end())
                continue;
            s += c->getName(true) + "->" + d->getName(true) + ";";
        }
    }
    s += "}";

    auto d = getServiceDir();
    write_file(d / "cyclic_deps.dot", s);

    throw SW_RUNTIME_EXCEPTION("Cannot create execution plan because of cyclic dependencies");
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

optional<FrontendType> Solution::selectFrontendByFilename(const path &fn)
{
    auto i = getAvailableFrontends().right.find(fn.filename());
    if (i == getAvailableFrontends().right.end())
        return {};
    return i->get_left();
}

Build::Build()
{
    //silent |= ide;

    /*static */const auto host_os = detectOS();

    Settings.HostOS = host_os;
    Settings.TargetOS = Settings.HostOS; // temp

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

void Build::setSettings()
{
    fs = &getFileStorage(getConfig());

    for (auto &[pp, m] : registered_programs)
        for (auto &[v,p] : m)
            p->fs = fs;

    if (Settings.Native.Librarian)
        Settings.Native.Librarian->fs = fs;
    if (Settings.Native.Linker)
        Settings.Native.Linker->fs = fs;
}

void Build::findCompiler()
{
    detectNativeCompilers(*this);

    using CompilerVector = std::vector<std::pair<PackagePath, CompilerType>>;

    auto activate = [this](const CompilerVector &a)
    {
        return std::any_of(a.begin(), a.end(), [this](const auto &v)
        {
            auto r = activateLanguage(v.first);
            if (r)
                this->Settings.Native.CompilerType = v.second;
            return r;
        });
    };

    auto activate_all = [this](const CompilerVector &a)
    {
        return std::all_of(a.begin(), a.end(), [this](const auto &v)
        {
            auto r = activateLanguage(v.first);
            if (r)
                this->Settings.Native.CompilerType = v.second;
            return r;
        });
    };

    auto activate_array = [&activate_all](const std::vector<CompilerVector> &a)
    {
        return std::any_of(a.begin(), a.end(), [&activate_all](const auto &v)
        {
            return activate_all(v);
        });
    };

    auto activate_or_throw = [&activate](const CompilerVector &a, const auto &e)
    {
        if (!activate(a))
            throw SW_RUNTIME_EXCEPTION(e);
    };

    auto activate_array_or_throw = [&activate_array](const std::vector<CompilerVector> &a, const auto &e)
    {
        if (!activate_array(a))
            throw SW_RUNTIME_EXCEPTION(e);
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
                this->Settings.Native.LinkerType = std::get<2>(v);
            }
            return r;
        }))
            throw SW_RUNTIME_EXCEPTION(e);
    };

    const CompilerVector msvc =
    {
        {"com.Microsoft.VisualStudio.VC.clpp", CompilerType::MSVC},
        {"com.Microsoft.VisualStudio.VC.cl", CompilerType::MSVC},
        {"com.Microsoft.VisualStudio.VC.ml", CompilerType::MSVC},
    };

    const CompilerVector gnu =
    {
        {"org.gnu.gcc.gpp", CompilerType::GNU},
        {"org.gnu.gcc.gcc", CompilerType::GNU},
        {"org.gnu.gcc.as", CompilerType::GNU}
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

    switch (Settings.Native.CompilerType)
    {
    case CompilerType::MSVC:
        activate_or_throw(msvc, "Cannot find msvc toolchain");
        break;
    case CompilerType::Clang:
        activate_or_throw(clang, "Cannot find clang toolchain");
        break;
    case CompilerType::ClangCl:
        activate_or_throw(clangcl, "Cannot find clang-cl toolchain");
        break;
    case CompilerType::GNU:
        activate_or_throw(gnu, "Cannot find gnu toolchain");
        break;
    case CompilerType::UnspecifiedCompiler:
        break;
    default:
        throw SW_RUNTIME_EXCEPTION("solution.cpp: not implemented");

    }

    if (Settings.Native.CompilerType == CompilerType::UnspecifiedCompiler)
    {
        switch (Settings.HostOS.Type)
        {
        case OSType::Windows:
            activate_array_or_throw({ msvc, clang, clangcl, }, "Try to add more compilers");
            break;
        case OSType::Linux:
        case OSType::Macos:
            activate_array_or_throw({ gnu, clang, }, "Try to add more compilers");
            break;
        }
    }

    if (Settings.TargetOS.Type != OSType::Macos)
    {
        extensions.erase(".m");
        extensions.erase(".mm");
    }

    activate_linker_or_throw({
        {"com.Microsoft.VisualStudio.VC.lib", "com.Microsoft.VisualStudio.VC.link",LinkerType::MSVC},
        {"org.gnu.binutils.ar", "org.gnu.gcc.ld",LinkerType::GNU},
        }, "Try to add more linkers");

    setSettings();
}

ExecutionPlan<builder::Command> Build::getExecutionPlan() const
{
    Commands cmds;
    for (auto &s : solutions)
    {
        auto c = s.getCommands();
        cmds.insert(c.begin(), c.end());
    }
    return Solution::getExecutionPlan(cmds);
}

void Build::performChecks()
{
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
    //performChecks();
    ScopedTime t;

    auto &e = getExecutor();
    std::vector<Future<void>> fs;
    for (auto &s : solutions)
        fs.push_back(e.push([&s] { s.prepare(); }, solutions.size()));
    waitAndGet(fs);

    if (!silent)
        LOG_INFO(logger, "Prepare time: " << t.getTimeFloat() << " s.");
}

Solution &Build::addSolution()
{
    return solutions.emplace_back(*this);
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
    return "loc.sw.self" "." + getFilesHash(files);
}

SharedLibraryTarget &Build::createTarget(const Files &files)
{
    auto &solution = solutions[0];
    solution.IsConfig = true;
    auto &lib = solution.addTarget<SharedLibraryTarget>(getSelfTargetName(files), "local");
    solution.IsConfig = false;
    //lib.PostponeFileResolving = false;
    return lib;
}

static void addDeps(NativeExecutedTarget &lib, Solution &solution)
{
    lib += solution.getTarget<NativeTarget>("pub.egorpugin.primitives.version");
    lib += solution.getTarget<NativeTarget>("pub.egorpugin.primitives.filesystem");

    auto d = lib + solution.getTarget<NativeTarget>("org.sw.sw.client.driver.cpp");
    d->IncludeDirectoriesOnly = true;
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

#if defined(CPPAN_OS_WINDOWS)
    auto &implib = solution.getImportLibrary();
#endif

    bool once = false;
    auto prepare_config = [this, &once,
#if defined(CPPAN_OS_WINDOWS)
            &implib,
#endif
            &solution
    ](const auto &fn)
    {
        auto &lib = createTarget({ fn });

        if (do_not_rebuild_config && fs::exists(lib.getOutputFile()))
            return lib.getOutputFile();

        do_not_rebuild_config = false;

        if (!once)
        {
            check_self(solution.Checks);
            solution.performChecks();
            build_self(solution);
            once = true;
        }

#if defined(CPPAN_OS_WINDOWS)
        lib += implib;
#endif
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
                throw SW_RUNTIME_EXCEPTION("pchs are not implemented for clang");
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
#else
        primitives::Command c;
        char buf[1024] = {0};
        pid_t pid = getpid();
        snprintf(buf, sizeof(buf), "/proc/%d/exe", pid);
        c.args = {"readlink", "-f", buf};
        c.execute();
        //lib.LinkLibraries.insert(c.out.text);
#endif

        if (auto L = lib.Linker->template as<VisualStudioLinker>())
        {
            L->DelayLoadDlls().push_back(IMPORT_LIBRARY);
            //#ifdef CPPAN_DEBUG
            L->GenerateDebugInfo = true;
            L->Force = vs::ForceType::Multiple;
            //#endif
        }

        addDeps(lib, solution);
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
        Solution::execute();

    return r;
}

path Build::build_configs(const std::unordered_set<ExtendedPackageData> &pkgs)
{
    if (pkgs.empty())
        return {};

    if (solutions.empty())
        addSolution();

    auto &solution = solutions[0];

    solution.Settings.Native.LibrariesType = LibraryType::Static;
    if (debug_configs)
        solution.Settings.Native.ConfigurationType = ConfigurationType::Debug;

#if defined(CPPAN_OS_WINDOWS)
    auto &implib = solution.getImportLibrary();
#endif

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

    if (do_not_rebuild_config && fs::exists(lib.getOutputFile()))
        return lib.getOutputFile();

    do_not_rebuild_config = false;

    check_self(solution.Checks);
    solution.performChecks();
    build_self(solution);

#if defined(CPPAN_OS_WINDOWS)
    lib += implib;
#endif
    lib.AutoDetectOptions = false;
    lib.CPPVersion = CPPLanguageStandard::CPP17;

    // separate loop
    for (auto &fn : files)
    {
        lib += fn;
        lib[fn].fancy_name = "[" + output_names[fn].toString() + "]/[config]";
        if (gVerbose)
            lib[fn].fancy_name += " (" + normalize_path(fn) + ")";
    }

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
            if (Settings.HostOS.Type != OSType::Windows)
                ctx.addLine("extern \"C\"");
            ctx.addLine("void build_" + h + "(Solution &);");
            if (Settings.HostOS.Type != OSType::Windows)
                ctx.addLine("extern \"C\"");
            ctx.addLine("void check_" + h + "(Checker &);");
            ctx.addLine();

            build.addLine("// " + r.toString());
            build.addLine("// " + normalize_path(fn));
            build.addLine("s.NamePrefix = \"" + r.ppath.slice(0, r.prefix).toString() + "\";");
            build.addLine("s.current_module = \"" + r.toString() + "\";");
            build.addLine("build_" + h + "(s);");
            build.addLine();

            auto cfg = read_file(fn);
            if (cfg.find("void check(") != cfg.npos)
            {
                check.addLine("// " + r.toString());
                check.addLine("check_" + h + "(c);");
                check.addLine();
            }
        }

        build.addLine("s.NamePrefix.clear();");
        build.endFunction();
        check.endFunction();

        ctx += build;
        ctx += check;

        auto p = many_files_fn = BinaryDir / "self" / ("sw." + h + ".cpp");
        write_file_if_different(p, ctx.getText());
        lib += p;
    }

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
                throw SW_RUNTIME_EXCEPTION("clang compiler is not implemented");

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
                    h = fn.parent_path() / ".sw" / "aux" / ("defs_" + hash + ".h");
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
            throw SW_RUNTIME_EXCEPTION("pchs are not implemented for clang");
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
#else
    primitives::Command c;
    char buf[1024] = { 0 };
    pid_t pid = getpid();
    snprintf(buf, sizeof(buf), "/proc/%d/exe", pid);
    c.args = { "readlink", "-f", buf };
    c.execute();
    //lib.LinkLibraries.insert(c.out.text);
#endif

    if (auto L = lib.Linker->template as<VisualStudioLinker>())
    {
        L->DelayLoadDlls().push_back(IMPORT_LIBRARY);
        //#ifdef CPPAN_DEBUG
        L->GenerateDebugInfo = true;
        L->Force = vs::ForceType::Multiple;
        //#endif
    }

    addDeps(lib, solution);

    auto i = solution.children.find(lib.pkg);
    if (i == solution.children.end())
        throw std::logic_error("config target not found");
    solution.TargetsToBuild[i->first] = i->second;

    Solution::execute();

    return lib.getOutputFile();
}

path Build::build(const path &fn)
{
    if (fs::is_directory(fn))
        throw SW_RUNTIME_EXCEPTION("Filename expected");

    auto fe = selectFrontendByFilename(fn);
    if (!fe)
        throw SW_RUNTIME_EXCEPTION("Unknown frontend config: " + fn.u8string());

    setupSolutionName(fn);
    config = fn;

    switch (fe.value())
    {
    case FrontendType::Sw:
    {
        // separate build
        Build b;
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

void Build::build_and_load(const path &fn)
{
    build(fn);
    //fs->save(); // remove?
    //fs->reset();
    auto fe = selectFrontendByFilename(fn);
    switch (fe.value())
    {
    case FrontendType::Sw:
        load(dll);
        break;
    case FrontendType::Cppan:
        cppan_load();
        break;
    }
}

ExecutionPlan<builder::Command> load(const path &fn, const Solution &s)
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
    return ExecutionPlan<builder::Command>::createExecutionPlan(commands2);
}

void save(const path &fn, const ExecutionPlan<builder::Command> &p)
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

    if (generateBuildSystem())
        return true;

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
                throw SW_RUNTIME_EXCEPTION("Empty target");
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

bool Build::load_configless(const path &file_or_dir)
{
    setupSolutionName(file_or_dir);

    load({}, false);

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

    return true;
}

void Build::build_and_run(const path &fn)
{
    build_and_load(fn);
    if (generateBuildSystem())
        return;
    Solution::execute();
}

bool Build::generateBuildSystem()
{
    if (generator.empty())
        return false;

    prepare();
    getCommands();

    auto g = Generator::create(generator);
    //g.file = fn.filename();
    //g.dir = fs::current_path();
    fs::remove_all(getExecutionPlansDir());
    g->generate(*this);
    return true;
}

void Build::build_package(const String &s)
{
    //auto [pkg,pkgs] = resolve_dependency(s);
    auto pkg = extractFromString(s);
    auto r = pkg.resolve();

    Local = false;
    NamePrefix = pkg.ppath.slice(0, r.prefix);
    build_and_run(r.getDirSrc2() / "sw.cpp");
}

void Build::run_package(const String &s)
{
    auto pkg = extractFromString(s);
    auto r = pkg.resolve();

    build_package(s);
    auto p = (NativeExecutedTarget*)solutions[0].getTargetPtr(r).get();
    if (p->getType() != TargetType::NativeExecutable)
        throw SW_RUNTIME_EXCEPTION("Unsupported package type");

    RunArgs a;
    a.pkg = r;
    a.exe_path = p->getOutputFile();
    //p->addCommand().c;
    a.in_container = false;
    run(a);
}

void Build::load(const path &dll, bool usedll)
{
    if (gWithTesting)
        with_testing = true;

    if (configure)
    {
        // explicit presets
        Settings.Native.LibrariesType = LibraryType::Shared;
        Settings.Native.ConfigurationType = ConfigurationType::Release;
        Settings.TargetOS.Arch = ArchType::x86_64;
#ifdef _WIN32
        Settings.Native.CompilerType = CompilerType::MSVC;
#else
        Settings.Native.CompilerType = CompilerType::GNU;
#endif
#ifdef _WIN32
        Settings.TargetOS.Type = OSType::Windows;
#elif __APPLE__
        Settings.TargetOS.Type = OSType::Macos;
#else
        Settings.TargetOS.Type = OSType::Linux;
#endif

        // configure may change defaults, so we must care below
        if (usedll)
            getModuleStorage(base_ptr).get(dll).configure(*this);

        if (boost::iequals(configuration, "Debug"))
            Settings.Native.ConfigurationType = ConfigurationType::Debug;
        else if (boost::iequals(configuration, "Release"))
            Settings.Native.ConfigurationType = ConfigurationType::Release;
        else if (boost::iequals(configuration, "MinSizeRel"))
            Settings.Native.ConfigurationType = ConfigurationType::MinimalSizeRelease;
        else if (boost::iequals(configuration, "RelWithDebInfo"))
            Settings.Native.ConfigurationType = ConfigurationType::ReleaseWithDebugInformation;

        if (static_build)
            Settings.Native.LibrariesType = LibraryType::Static;
        if (shared_build)
            Settings.Native.LibrariesType = LibraryType::Shared;

        if (boost::iequals(platform, "Win32"))
            Settings.TargetOS.Arch = ArchType::x86;
        else if (boost::iequals(platform, "Win64"))
            Settings.TargetOS.Arch = ArchType::x86_64;
        else if (boost::iequals(platform, "arm32"))
            Settings.TargetOS.Arch = ArchType::arm;
        else if (boost::iequals(platform, "arm64"))
            Settings.TargetOS.Arch = ArchType::aarch64; // ?

        if (boost::iequals(compiler, "clang"))
            Settings.Native.CompilerType = CompilerType::Clang;
        else if (boost::iequals(compiler, "clangcl") || boost::iequals(compiler, "clang-cl"))
            Settings.Native.CompilerType = CompilerType::ClangCl;
        else if (boost::iequals(compiler, "gnu"))
            Settings.Native.CompilerType = CompilerType::GNU;
        else if (boost::iequals(compiler, "msvc"))
            Settings.Native.CompilerType = CompilerType::MSVC;
        else if (!compiler.empty())
            throw SW_RUNTIME_EXCEPTION("unknown compiler: " + compiler);

        if (boost::iequals(target_os, "linux"))
            Settings.TargetOS.Type = OSType::Linux;
        else if (boost::iequals(target_os, "macos"))
            Settings.TargetOS.Type = OSType::Macos;
        else if (boost::iequals(target_os, "windows") || boost::iequals(target_os, "win"))
            Settings.TargetOS.Type = OSType::Windows;
    }

    // apply config settings
    findCompiler();

    if (solutions.empty())
        addSolution();

    // check
    {
        // some packages want checks in their build body
        // because they use variables from checks

        // make parallel?
        if (usedll)
        {
            for (auto &s : solutions)
                getModuleStorage(base_ptr).get(dll).check(s, s.Checks);
        }
        performChecks();
    }

    // build
    if (usedll)
    {
        for (auto &s : solutions)
            getModuleStorage(base_ptr).get(dll).build(s);
    }

    // we build only targets from this package
    for (auto &s : solutions)
        s.TargetsToBuild = s.children;
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

        auto nt = (NativeExecutedTarget*)t.get();

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
                throw SW_RUNTIME_EXCEPTION("no such source");
            rd = si->second;
        }
        j["root_dir"] = rd.u8string();

        // files
        // we do not use nt->gatherSourceFiles(); as it removes deleted files
        Files files;
        for (auto &f : nt->gatherAllFiles())
        {
            if (File(f, *fs).isGeneratedAtAll())
                continue;
            files.insert(f);
        }

        if (files.empty() && !nt->Empty)
            throw SW_RUNTIME_EXCEPTION(pkg.toString() + ": No files found");
        if (!files.empty() && nt->Empty)
            throw SW_RUNTIME_EXCEPTION(pkg.toString() + ": Files were found, but target is marked as empty");

        // we put files under SW_SDIR_NAME to keep space near it
        // e.g. for patch dir or other dirs (server provided files)
        // we might unpack to other dir, but server could push service files in neighbor dirs like gpg keys etc
        nlohmann::json jm;
        // 'from' field is calculated relative to fetch/sln dir
        auto files_map1 = primitives::pack::prepare_files(files, rd);
        // but 'to' field is calculated based on target's own view
        auto files_map2 = primitives::pack::prepare_files(files, t->SourceDir);
        for (const auto &tup : boost::combine(files_map1, files_map2))
        {
            std::pair<path, path> f1, f2;
            boost::tie(f1, f2) = tup;

            nlohmann::json jf;
            jf["from"] = f1.first.u8string();
            jf["to"] = f2.second.u8string();
            j["files"].push_back(jf);
        }

        // deps
        DependenciesType deps;
        nt->TargetOptionsGroup::iterate<WithoutSourceFileStorage, WithNativeOptions>([&deps](auto &v, auto &gs)
        {
            deps.insert(v.Dependencies.begin(), v.Dependencies.end());
        });
        for (auto &d : deps)
        {
            if (d->target.lock() && d->target.lock()->Scope != TargetScope::Build)
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
