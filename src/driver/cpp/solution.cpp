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
#include "program.h"
#include "resolver.h"
#include "run.h"

#include <directories.h>
#include <hash.h>
#include <settings.h>

#include <boost/algorithm/string.hpp>
#include <boost/thread/lock_types.hpp>
#include <primitives/context.h>
#include <primitives/date_time.h>
#include <primitives/executor.h>
#include <primitives/pack.h>
#include <primitives/templates.h>
#include <primitives/win32helpers.h>
#include <primitives/sw/settings.h>

#include <boost/dll.hpp>
#include <nlohmann/json.hpp>

#ifndef _WIN32
#define _GNU_SOURCE // for dladdr
#include <dlfcn.h>
#endif

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "target");

void build_self(sw::Solution &s);
void check_self(sw::Checker &c);

static cl::opt<bool> print_commands("print-commands", cl::desc("Print file with build commands"));
static cl::opt<String> generator("G", cl::desc("Generator"));
static cl::opt<bool> do_not_rebuild_config("do-not-rebuild-config", cl::Hidden);
static cl::opt<bool> dry_run("n", cl::desc("Dry run"));
static cl::opt<bool> debug_configs("debug-configs", cl::desc("Build configs in debug mode"));

static cl::opt<String> target_os("target-os");
static cl::opt<String> compiler("compiler", cl::desc("Set compiler")/*, cl::sub(subcommand_ide)*/);
static cl::opt<String> configuration("configuration", cl::desc("Set build configuration")/*, cl::sub(subcommand_ide)*/);
static cl::opt<String> platform("platform", cl::desc("Set build platform")/*, cl::sub(subcommand_ide)*/);
//static cl::opt<String> arch("arch", cl::desc("Set arch")/*, cl::sub(subcommand_ide)*/);
static cl::opt<bool> static_build("static-build", cl::desc("Set static build")/*, cl::sub(subcommand_ide)*/);

namespace sw
{

void *getModuleForSymbol(void *f)
{
#ifdef _WIN32
    HMODULE hModule = NULL;
    // hModule is NULL if GetModuleHandleEx fails.
    GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
        | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCTSTR)(f ? f : getCurrentModuleSymbol()), &hModule);
    return hModule;
#else
    Dl_info i;
    if (dladdr(f ? f : getCurrentModuleSymbol(), &i))
        return i.dli_fbase;
    return nullptr;
#endif
}

path getModuleNameForSymbol(void *f)
{
#ifdef _WIN32
    auto lib = getModuleForSymbol(f);
    const auto sz = 1 << 16;
    WCHAR n[sz] = { 0 };
    GetModuleFileNameW((HMODULE)lib, n, sz);
    path m = n;
    return m;// .filename();
#else
    if (!f)
        return boost::dll::program_location().string();
    Dl_info i;
    if (dladdr(f ? f : getCurrentModuleSymbol(), &i))
        return fs::absolute(i.dli_fname);
    return {};
#endif
}

static path getCurrentModuleName()
{
    return getModuleNameForSymbol();
}

String getCurrentModuleNameHash()
{
    return shorten_hash(blake2b_512(getCurrentModuleName().u8string()));
}

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
        throw std::runtime_error("No header for package: " + p.toString());
    f = f.substr(pos + sizeof(on));
    pos = f.find("#pragma sw header off");
    if (pos == f.npos)
        throw std::runtime_error("No end in header for package: " + p.toString());
    f = f.substr(0, pos);
    //if (std::regex_search(f, m, r_header))
    {
        Context ctx;
        ctx.addLine("#pragma once");
        ctx.addLine();

        Context prefix;
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

ModuleStorage &getModuleStorage()
{
    static ModuleStorage modules;
    return modules;
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
{
    Checks.solution = this;
}

Solution::~Solution()
{
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
    return BinaryDir / "sln" / compiler_name;
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

StaticLibraryTarget &Solution::getImportLibrary()
{
#if defined(CPPAN_OS_WINDOWS)
    HMODULE lib = (HMODULE)getModuleForSymbol();
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

    throw std::runtime_error("Cannot create execution plan because of cyclic dependencies");
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
    for (auto &p : chldr)
    {
        auto c = p.second->getCommands();
        for (auto &c2 : c)
            c2->maybe_unused &= ~builder::Command::MU_TRUE;
        cmds.insert(c.begin(), c.end());
    }

    return cmds;
}

Files Solution::getGeneratedDirs() const
{
    Files f;
    for (auto &p : getChildren())
    {
        auto c = p.second->getGeneratedDirs();
        f.insert(c.begin(), c.end());
    }
    return f;
}

void Solution::createGeneratedDirs() const
{
    for (auto &d : getGeneratedDirs())
        fs::create_directories(d);
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

    for (auto &c : p.commands)
        c->silent = silent;

    size_t current_command = 1;
    size_t total_commands = 0;
    for (auto &c : p.commands)
    {
        if (!c->outputs.empty())
            total_commands++;
    }

    for (auto &c : p.commands)
    {
        c->total_commands = total_commands;
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

void Solution::prepare()
{
    if (prepared)
        return;

    static int recursion_count = 0;
    recursion_count++;
    SCOPE_EXIT
    {
        recursion_count--;
    };

    if (recursion_count > 30)
        LOG_ERROR(logger, "recursion detected: " << recursion_count);
    if (recursion_count > 45)
        throw std::logic_error("stopping recursion");

    // all targets are set stay unchanged from user
    // so, we're ready to some preparation passes

    // resolve all deps first
    if (auto ud = gatherUnresolvedDependencies(); !ud.empty())
    {
        // first round
        UnresolvedPackages pkgs;
        for (auto &[pkg, d] : ud)
            pkgs.insert(pkg);

        // resolve only deps needed
        Resolver r;
        r.resolve_dependencies(pkgs, true);
        auto dd = r.getDownloadDependencies();
        if (dd.empty())
            throw std::runtime_error("Empty download dependencies");

        for (auto &p : dd)
            knownTargets.insert(p);

        // gather packages
        std::unordered_map<PackageVersionGroupNumber, ExtendedPackageData> cfgs2;
        for (auto &[p, gn] : r.getDownloadDependenciesWithGroupNumbers())
            cfgs2[gn] = p;
        std::unordered_set<ExtendedPackageData> cfgs;
        for (auto &[gn, s] : cfgs2)
            cfgs.insert(s);

        Build b;
        b.Local = false;
        auto dll = b.build_configs(cfgs);

        Local = false;

        SwapAndRestore sr(NamePrefix, cfgs.begin()->ppath.slice(0, cfgs.begin()->prefix));
        if (cfgs.size() != 1)
            sr.restoreNow(true);

        getModuleStorage(base_ptr).get(dll).check(Checks);
        performChecks();
        getModuleStorage(base_ptr).get(dll).build(*this);

        sr.restoreNow(true);

        int retries = 0;
        while (!ud.empty())
        {
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

            ud = gatherUnresolvedDependencies();
            UnresolvedPackages pkgs;
            for (auto &[pkg, d] : ud)
                pkgs.insert(pkg);
            r.resolve_dependencies(pkgs);
        }
    }

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

    // move to prepare?
    createGeneratedDirs();

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
        throw std::runtime_error("Prepare solution before executing");
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

    throw std::runtime_error("Cannot create execution plan because of cyclic dependencies");
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

    // maybe also clear checks?
    // or are they solution-specific?

    // do not clear modules on exception, because it may come from there
    if (!std::uncaught_exceptions())
        getModuleStorage(base_ptr).modules.clear();
}

void Build::setSettings()
{
    /*Settings.Native.ASMCompiler = ((ASMLanguage*)languages[LanguageType::ASM].get())->compiler;
    Settings.Native.CCompiler = ((CLanguage*)languages[LanguageType::C].get())->compiler;
    Settings.Native.CPPCompiler = ((CPPLanguage*)languages[LanguageType::CPP].get())->compiler;*/

    //Settings.Native.CompilerType = ((CPPLanguage*)languages[LanguageType::CPP].get())->compiler->Type;

    //throw std::runtime_error("todo");
    //Settings.Native.CompilerType = ((CPPLanguage*)user_defined_languages[extensions[".cpp"]].get())->compiler->Type;

    fs = &getFileStorage(getConfig());

    /*Settings.Native.ASMCompiler->fs = fs;
    Settings.Native.CCompiler->fs = fs;
    Settings.Native.CPPCompiler->fs = fs;*/
    /*if (Settings.Native.Librarian)
        Settings.Native.Librarian->fs = fs;
    if (Settings.Native.Linker)
        Settings.Native.Linker->fs = fs;*/

/*#define SET_FS(type)                                                             \
    ((type##Language *)languages[LanguageType::type].get())->compiler->fs = fs;  \
    ((type##Language *)languages[LanguageType::type].get())->librarian->fs = fs; \
    ((type##Language *)languages[LanguageType::type].get())->linker->fs = fs

    SET_FS(ASM);
    SET_FS(C);
    SET_FS(CPP);*/

    /*for (auto &lang : getLanguages())
        for (auto &l : lang->CompiledExtensions)
            extensions[l] = user_defined_languages[lang];
    setExtensionLanguage()*/

    for (auto &[pp, m] : registered_programs)
        for (auto &[v,p] : m)
        p->fs = fs;
}

void Build::findCompiler()
{
    detectNativeCompilers(*this);

    auto activate_or_throw = [this](const std::vector<PackagePath> &a, const auto &e)
    {
        if (!std::any_of(a.begin(), a.end(), [this](const auto &v)
        {
            return activateLanguage(v);
        }))
            throw std::runtime_error(e);
    };

    switch (Settings.Native.CompilerType)
    {
    case CompilerType::MSVC:
    {
        activate_or_throw(
            { "com.Microsoft.VisualStudio.VC.cl" },
            "Cannot find msvc toolchain");
        break;
    }
    case CompilerType::Clang:
    {
        //if (!Clang().findToolchain(*this))
            throw std::runtime_error("Cannot find clang toolchain");
        break;
    }
    case CompilerType::ClangCl:
    {
        //if (!ClangCl().findToolchain(*this))
            throw std::runtime_error("Cannot find clang-cl toolchain");
        break;
    }
    case CompilerType::GNU:
    {
        //if (!GNU().findToolchain(*this))
            throw std::runtime_error("Cannot find gnu toolchain");
        break;
    }
    case CompilerType::UnspecifiedCompiler:
        break;
    default:
        throw std::runtime_error("solution.cpp: not implemented");

    }

    if (Settings.Native.CompilerType == CompilerType::UnspecifiedCompiler)
    {
        switch (Settings.HostOS.Type)
        {
        case OSType::Windows:
            activate_or_throw({
                "com.Microsoft.VisualStudio.VC.cl",
                "org.LLVM.clang",
                "org.LLVM.clangcl",
                }, "Try to add more compilers");
            break;
        case OSType::Linux:
            /*if (
                !GNU().findToolchain(*this) &&
                !Clang().findToolchain(*this) &&
                1
                )*/
            {
                throw std::runtime_error("Try to add more compilers");
            }
            //if (FileTransforms.IsEmpty())
            break;
        case OSType::Macos:
            /*if (
                !GNU().findToolchain(*this) &&
                !Clang().findToolchain(*this) && // does not support fs at the moment
                1
                )*/
            {
                throw std::runtime_error("Try to add more compilers");
            }
            //if (FileTransforms.IsEmpty())
            break;
        }
    }

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
        LOG_INFO(logger, "Checks time: " << t.getTimeFloat() << " s.");
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

    if (!do_not_rebuild_config)
    {
        check_self(solution.Checks);
        solution.performChecks();
        build_self(solution);
    }

    auto prepare_config = [this,
#if defined(CPPAN_OS_WINDOWS)
            &implib,
#endif
            &solution
    ](const auto &fn)
    {
        auto &lib = createTarget({ fn });
        if (do_not_rebuild_config)
            return lib.getOutputFile();
#if defined(CPPAN_OS_WINDOWS)
        lib += implib;
#endif
        lib.AutoDetectOptions = false;
        lib.CPPVersion = CPPLanguageStandard::CPP17;

        lib += fn;
        write_file_if_different(getImportPchFile(), cppan_cpp);
        lib.addPrecompiledHeader("sw/driver/cpp/sw.h", getImportPchFile());
        if (auto s = lib[getImportPchFile()].template as<CPPSourceFile>())
        {
            if (auto C = s->compiler->template as<VisualStudioCompiler>())
            {
                path of = getImportPchFile();
                of += ".obj";
                //C->setOutputFile(of);
            }
        }

        auto [headers, udeps] = getFileDependencies(fn);

        for (auto &h : headers)
        {
            if (auto sf = lib[fn].template as<CPPSourceFile>())
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

    if (!do_not_rebuild_config)
    {
        check_self(solution.Checks);
        solution.performChecks();
        build_self(solution);
    }

    Files files;
    std::unordered_map<path, PackageId> output_names;
    for (auto &pkg : pkgs)
    {
        auto p = pkg.getDirSrc2() / getConfigFilename();
        files.insert(p);
        output_names[p] = pkg;
    }
    bool many_files = files.size() > 1;
    auto h = getFilesHash(files);

    auto &lib = createTarget(files);
    if (do_not_rebuild_config)
        return lib.getOutputFile();
#if defined(CPPAN_OS_WINDOWS)
    lib += implib;
#endif
    lib.AutoDetectOptions = false;
    lib.CPPVersion = CPPLanguageStandard::CPP17;

    // separate loop
    for (auto &fn : files)
    {
        lib += fn;
        lib[fn].fancy_name = "building config [" + output_names[fn].toString() + "]/sw.cpp (" + normalize_path(fn) + ")";
    }

    // generate main source file
    if (many_files)
    {
        CppContext ctx;

        CppContext build;
        build.beginFunction("void build(Solution &s)");

        CppContext check;
        check.beginFunction("void check(Checker &c)");

        for (auto &r : pkgs)
        {
            auto fn = r.getDirSrc2() / getConfigFilename();
            auto h = getFilesHash({ fn });
            ctx.addLine("// " + r.toString());
            if (Settings.HostOS.Type != OSType::Windows)
                ctx.addLine("extern \"C\"");
            ctx.addLine("void build_" + h + "(Solution &);");
            if (Settings.HostOS.Type != OSType::Windows)
                ctx.addLine("extern \"C\"");
            ctx.addLine("void check_" + h + "(Checker &);");
            ctx.addLine();

            build.addLine("// " + r.toString());
            build.addLine("s.NamePrefix = \"" + r.ppath.slice(0, r.prefix).toString() + "\";");
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

        auto p = getDirectories().storage_dir_tmp / "self" / ("sw." + h + ".cpp");
        write_file_if_different(p, ctx.getText());
        lib += p;
    }

    // after files
    write_file_if_different(getImportPchFile(), cppan_cpp);
    lib.addPrecompiledHeader("sw/driver/cpp/sw.h", getImportPchFile());
    if (auto s = lib[getImportPchFile()].template as<CPPSourceFile>())
    {
        if (auto C = s->compiler->template as<VisualStudioCompiler>())
        {
            path of = getImportPchFile();
            of += ".obj";
            //C->setOutputFile(of);
        }
    }

    for (auto &fn : files)
    {
        auto[headers, udeps] = getFileDependencies(fn);
        if (auto sf = lib[fn].template as<CPPSourceFile>())
        {
            if (many_files)
            {
                if (auto c = sf->compiler->template as<NativeCompiler>())
                {
                    auto h = getFilesHash({ fn });
                    c->Definitions["configure"] = "configure_" + h;
                    c->Definitions["build"] = "build_" + h;
                    c->Definitions["check"] = "check_" + h;
                }
            }

            if (auto c = sf->compiler->template as<VisualStudioCompiler>())
            {
                for (auto &h : headers)
                    c->ForcedIncludeFiles().push_back(h);
            }
            else if (auto c = sf->compiler->template as<ClangClCompiler>())
            {
                for (auto &h : headers)
                    c->ForcedIncludeFiles().push_back(h);
            }
            else if (auto c = sf->compiler->template as<ClangCompiler>())
            {
                for (auto &h : headers)
                    c->ForcedIncludeFiles().push_back(h);
            }
            else if (auto c = sf->compiler->template as<GNUCompiler>())
            {
                for (auto &h : headers)
                    c->ForcedIncludeFiles().push_back(h);
            }
        }
        for (auto &d : udeps)
            lib += std::make_shared<Dependency>(d);
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

    if (!do_not_rebuild_config)
        Solution::execute();

    return lib.getOutputFile();
}

path Build::build(const path &fn)
{
    // separate build
    Build b;
    auto r = b.build_configs_separate({ fn });
    dll = r.begin()->second;
    if (File(dll, *b.solutions[0].fs).isChanged())
    {
        do_not_rebuild_config = false;
        return build(fn);
    }
    return dll;
}

void Build::build_and_load(const path &fn)
{
    build(fn);
    //fs->save(); // remove?
    //fs->reset();
    load(dll);
}

ExecutionPlan<builder::Command> load(const path &fn, const Solution &s)
{
    BinaryContext ctx;
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
    BinaryContext ctx;

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

    try
    {
        prepare();

        for (auto &[n, _] : TargetsToBuild)
        {
            for (auto &s : solutions)
            {
                auto &t = s.children[n];
                if (!t)
                    throw std::runtime_error("Empty target");
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
        return true;
    }
    catch (std::exception &e) { LOG_ERROR(logger, "error during build: " << e.what()); }
    catch (...) {}
    return false;
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
        throw std::runtime_error("Unsupported package type");

    RunArgs a;
    a.pkg = r;
    a.exe_path = p->getOutputFile();
    //p->addCommand().c;
    a.in_container = false;
    run(a);
}

void Build::load(const path &dll)
{
    if (configure)
    {
        // configure may change defaults, so we must care below
        getModuleStorage(base_ptr).get(dll).configure(*this);

        if (boost::iequals(configuration, "Debug"))
            Settings.Native.ConfigurationType = ConfigurationType::Debug;
        else if (boost::iequals(configuration, "Release"))
            Settings.Native.ConfigurationType = ConfigurationType::Release;
        else if (boost::iequals(configuration, "MinSizeRel"))
            Settings.Native.ConfigurationType = ConfigurationType::MinimalSizeRelease;
        else if (boost::iequals(configuration, "RelWithDebInfo"))
            Settings.Native.ConfigurationType = ConfigurationType::ReleaseWithDebugInformation;
        else // explicit else!
            Settings.Native.ConfigurationType = ConfigurationType::Release;
        //if (!platform.empty())
            //;

        if (static_build)
            Settings.Native.LibrariesType = LibraryType::Static;
        else // explicit else!
            Settings.Native.LibrariesType = LibraryType::Shared;

        if (boost::iequals(platform, "Win32"))
            Settings.TargetOS.Arch = ArchType::x86;
        else if (boost::iequals(platform, "Win64"))
            Settings.TargetOS.Arch = ArchType::x86_64;
        else if (boost::iequals(platform, "arm32"))
            Settings.TargetOS.Arch = ArchType::arm;
        else if (boost::iequals(platform, "arm64"))
            Settings.TargetOS.Arch = ArchType::aarch64; // ?
        else // explicit else!
            Settings.TargetOS.Arch = ArchType::x86_64;

        if (boost::iequals(compiler, "clang"))
            Settings.Native.CompilerType = CompilerType::Clang;
        else if (boost::iequals(compiler, "clang-cl"))
            Settings.Native.CompilerType = CompilerType::ClangCl;
        else if (boost::iequals(compiler, "gnu"))
            Settings.Native.CompilerType = CompilerType::GNU;
        else if (boost::iequals(compiler, "msvc"))
            Settings.Native.CompilerType = CompilerType::MSVC;
        else // explicit else!
#ifdef _WIN32
            Settings.Native.CompilerType = CompilerType::MSVC;
#else
            Settings.Native.CompilerType = CompilerType::GNU;
#endif

        if (boost::iequals(target_os, "linux"))
            Settings.TargetOS.Type = OSType::Linux;
        else if (boost::iequals(target_os, "macos"))
            Settings.TargetOS.Type = OSType::Macos;
        else if (boost::iequals(target_os, "windows") || boost::iequals(target_os, "win"))
            Settings.TargetOS.Type = OSType::Windows;
        else // explicit else!
#ifdef _WIN32
            Settings.TargetOS.Type = OSType::Windows;
#elif __APPLE__
            Settings.TargetOS.Type = OSType::Macos;
#else
            Settings.TargetOS.Type = OSType::Linux;
#endif
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
        for (auto &s : solutions)
            getModuleStorage(base_ptr).get(dll).check(s.Checks);
        performChecks();
    }

    // build
    for (auto &s : solutions)
    {
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
    for (auto &[pkg, t] : solutions.begin()->children)
    {
        if (t->Scope != TargetScope::Build)
            continue;

        auto nt = (NativeExecutedTarget*)t.get();

        nlohmann::json j;

        // source, version, path
        save_source(j["source"], t->source);
        j["version"] = pkg.getVersion().toString();
        j["path"] = pkg.ppath.toString();

        j["root_dir"] = t->SourceDir.u8string();

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
            throw std::runtime_error("No files found");
        if (!files.empty() && nt->Empty)
            throw std::runtime_error("Files were found, but target is marked as empty");

        // we put files under SW_SDIR_NAME to keep space near it
        // e.g. for patch dir or other dirs (server provided files)
        // we might unpack to other dir, but server could push service files in neighbor dirs like gpg keys etc
        auto files_map = primitives::pack::prepare_files(files, t->SourceDir, SW_SDIR_NAME);
        for (auto &[f,t] : files_map)
        {
            nlohmann::json jf;
            jf["from"] = f.u8string();
            jf["to"] = t.u8string();
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
            jd["versions"] = d->getPackage().range.toString();
            j["dependencies"].push_back(jd);
        }

        auto s = j.dump();
        m[pkg] = std::make_unique<JsonPackageDescription>(s);
    }
    return m;
}

const Module &ModuleStorage::get(const path &dll)
{
    if (dll.empty())
        throw std::runtime_error("Empty module");

    boost::upgrade_lock lk(m);
    auto i = modules.find(dll);
    if (i != modules.end())
        return i->second;
    boost::upgrade_to_unique_lock lk2(lk);
    return modules.emplace(dll, dll).first->second;
}

Module::Module(const path &dll)
try
    : module(new boost::dll::shared_library(dll.wstring(),
        boost::dll::load_mode::rtld_now | boost::dll::load_mode::rtld_global))
{
    if (module->has("build"))
        build_ = module->get<void(Solution&)>("build");
    if (module->has("check"))
        check = module->get<void(Checker&)>("check");
    if (module->has("configure"))
        configure = module->get<void(Solution&)>("configure");
}
catch (...)
{
    LOG_ERROR(logger, "Module " + normalize_path(dll) + " is in bad shape. Will rebuild on the next run.");
    fs::remove(dll);
}

Module::~Module()
{
//#ifdef _WIN32
    delete module;
//#endif
}

void Module::build(Solution &s) const
{
    //Solution s2(s);
    //build_(s2);
    build_(s);
    /*for (auto &[p, t] : s2.children)
    {
        if (s.knownTargets.find(p) == s.knownTargets.end())
            continue;
        s.add(t);
    }*/
}

ModuleStorage &getModuleStorage(Solution &owner)
{
    static std::map<void*, ModuleStorage> s;
    return s[&owner];
}

}
