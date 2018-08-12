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

#include <directories.h>
#include <hash.h>
#include <settings.h>

#include <primitives/context.h>
#include <primitives/date_time.h>
#include <primitives/executor.h>
#include <primitives/pack.h>
#include <primitives/templates.h>
#include <primitives/win32helpers.h>

#include <boost/dll.hpp>
#include <nlohmann/json.hpp>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "target");

void build_self(sw::Solution &s);
void check_self(sw::Checker &c);

namespace sw
{

/*template <class T>
struct ScopedValue
{
    T &old_ref;
    T old;

    ScopedValue(T &v)
        : old_ref(v), old(std::move(v))
    {
        v = T();
    }

    ScopedValue(T &v, const T &nv)
        : old_ref(v), old(std::move(v))
    {
        v = nv;
    }

    ScopedValue(T &v, T &&nv)
        : old_ref(v), old(std::move(v))
    {
        v = std::move(nv);
    }

    ~ScopedValue()
    {
        //old_ref = std::move(old);
        old_ref.insert(old.begin(), old.end());
    }
};*/

#ifdef _WIN32
static HMODULE GetCurrentModule()
{
    HMODULE hModule = NULL;
    // hModule is NULL if GetModuleHandleEx fails.
    GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
        | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCTSTR)GetCurrentModule, &hModule);
    return hModule;
}

static String GetCurrentModuleName()
{
    auto lib = GetCurrentModule();
    const auto sz = 1 << 16;
    WCHAR n[sz] = { 0 };
    GetModuleFileNameW(lib, n, sz);
    path m = n;
    return m.filename().u8string();
}
#else
static String GetCurrentModuleName()
{
    return {};
}
#endif

static String GetCurrentModuleNameHash()
{
    return shorten_hash(blake2b_512(GetCurrentModuleName()));
}

path getImportFilePrefix()
{
    return getUserDirectories().storage_dir_tmp / ("cppan_" + GetCurrentModuleNameHash());
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

        ctx.addLine("#define THIS_PREFIX \"" + p.ppath.slice(0, p.prefix).toString() + "\"");
        ctx.addLine("#define THIS_RELATIVE_PACKAGE_PATH \"" + p.ppath.slice(p.prefix).toString() + "\"");
        ctx.addLine("#define THIS_PACKAGE_PATH THIS_PREFIX \".\" THIS_RELATIVE_PACKAGE_PATH");
        ctx.addLine("#define THIS_VERSION \"" + p.version.toString() + "\"");
        ctx.addLine("#define THIS_VERSION_DEPENDENCY \"" + p.version.toString() + "\"_dep");
        ctx.addLine("#define THIS_PACKAGE THIS_PACKAGE_PATH \"-\" THIS_VERSION");
        ctx.addLine("#define THIS_PACKAGE_DEPENDENCY THIS_PACKAGE_PATH \"-\" THIS_VERSION_DEPENDENCY");
        ctx.addLine();

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

UnresolvedPackages getFileDependencies(const path &p)
{
    UnresolvedPackages deps;
    return deps;
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

    SourceDir = current_thread_path();
    BinaryDir = SourceDir / ".sw";
}

Solution::Solution(const Solution &rhs)
    : TargetBase(rhs)
    , HostOS(rhs.HostOS)
    //, checksStorage(rhs.checksStorage)
    , silent(rhs.silent)
    , base_ptr(rhs.base_ptr)
    //, knownTargets(rhs.knownTargets)
    , source_dirs_by_source(rhs.source_dirs_by_source)
{
    Checks.solution = this;
    //if (!Version.isValid())
        //Version = Local ? "local" : "master";
}

/*Solution &Solution::operator=(const Solution &rhs)
{
    Solution tmp(rhs);
    std::swap(*this, tmp);
    return *this;
}*/

Solution::~Solution()
{
}

bool Solution::isKnownTarget(const PackageId &p) const
{
    return knownTargets.empty() || knownTargets.find(p) != knownTargets.end();
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

StaticLibraryTarget &Solution::getImportLibrary()
{
    //auto c = n\ew ExecuteCommand(IMPORT_DEFINITIONS_FILE, []
    //{
#if defined(CPPAN_OS_WINDOWS)
    HMODULE lib = GetCurrentModule();
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
    //});
    //File(IMPORT_DEFINITIONS_FILE).getFileRecord().generator = c;

    //auto o = BinaryDir;
    //BinaryDir = getUserDirectories().storage_dir_tmp;
    auto o = Local;
    Local = false; // this prevents us from putting compiled configs into user bdirs
    IsConfig = true;
    auto &t = addTarget<StaticLibraryTarget>("cppan_implib_" + GetCurrentModuleNameHash(), "local");
    //t.init2();
    IsConfig = false;
    Local = o;
    t.AutoDetectOptions = false;
    //fs::create_directories(t.BinaryDir);
    //BinaryDir = o;
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
        p.second->getCommands();

    Commands cmds;
    auto &chldr = TargetsToBuild.empty() ? children : TargetsToBuild;
    for (auto &p : chldr)
    {
        auto c = p.second->getCommands();
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
                s += "\"" + p.target_name + "\"->\"" + d->target->pkg.target_name + "\";\n";
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
            {
                auto n = programs.size() + 1;
                if (programs.find(c->getProgram()) == programs.end())
                    programs[c->getProgram()] = n;
            }
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

    auto p = getExecutionPlan();

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
    if (::sw::Settings::get_local_settings().print_commands && !silent) // && !b console mode
    {
        auto d = getServiceDir();

        //message_box(d.string());
        print_graph(p, d / "build.dot");
        printGraph(d / "solution.dot");
        print_commands(p, d / "commands.bat");
    }

    ScopedTime t;

    /*std::sort(p.commands.begin(), p.commands.end(),
        [](const auto &c1, const auto &c2)
    {
        if (c1->dependencies.size() < c2->dependencies.size())
            return true;
        else if (c1->dependencies.size() > c2->dependencies.size())
            return false;
        return c1->getName() < c2->getName();
    });*/
    //Executor e(1);
    //p.execute(e);

    //Executor e(1);
    auto &e = getExecutor();

    p.execute(e);
    if (!silent)
        LOG_INFO(logger, "Build time: " << t.getTimeFloat() << " s.");
}

void Solution::prepare()
{
    if (prepared)
        return;

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
        r.resolve_dependencies(pkgs);
        auto dd = r.getDownloadDependencies();

        for (auto &p : dd)
            knownTargets.insert(p);

        // always try to take first path
        std::unordered_map<PackageVersionGroupNumber, std::set<path>> cfgs2;
        for (auto &[p, gn] : r.getDownloadDependenciesWithGroupNumbers())
            cfgs2[gn].insert(p.getDirSrc2() / getConfigFilename());

        Files cfgs;
        for (auto &[gn, s] : cfgs2)
            cfgs.insert(*s.begin());

        Build b;
        b.Local = false;
        auto dlls = b.build_configs(cfgs);

        // make parallel

        Local = false;
        //s.knownTargets = knownTargets;

        // on the first step we load configure information
        for (auto &p : dd)
        {
            if (children.find(p) != children.end())
                continue;

            auto f = p.getDirSrc2() / getConfigFilename();
            if (cfgs.find(f) == cfgs.end())
                continue;
            auto dll = dlls[f];

            NamePrefix = p.ppath.slice(0, 2);
            getModuleStorage(base_ptr).get(dll).check(Checks);

            /*auto &c = b2.solutions[0].getChildren();
            for (auto &[pp, t] : c)
            t->solution = &solutions[0];
            solutions[0].children.insert(c.begin(), c.end());
            solutions[0].copyChecksFrom(b2.solutions[0]);
            b2.solutions[0].children.clear();*/
        }

        performChecks();

        for (auto &p : dd)
        {
            if (children.find(p) != children.end())
                continue;

            auto f = p.getDirSrc2() / getConfigFilename();
            if (cfgs.find(f) == cfgs.end())
                continue;
            auto dll = dlls[f];

            NamePrefix = p.ppath.slice(0, 2);
            //ScopedValue<decltype(children)> c(children);
            getModuleStorage(base_ptr).get(dll).build(*this);

            /*auto &c = b2.solutions[0].getChildren();
            for (auto &[pp, t] : c)
            t->solution = &solutions[0];
            solutions[0].children.insert(c.begin(), c.end());
            solutions[0].copyChecksFrom(b2.solutions[0]);
            b2.solutions[0].children.clear();*/
        }

        // make parallel
        /*for (auto &p : dd)
        {
            auto f = p.getDirSrc2() / getConfigFilename();
            auto dll = dlls[f];

            //Build b;
            //b.silent = true;
            //b.Local = false;
            //if (File(f).isChanged() || File(boost::dll::program_location()).isChanged())
            //dll = b.build_configs({ f }).begin()->second;
            /*else
            {
                dll = b.get_module_name(f);
                if (!fs::exists(dll))
                    dll = b.build(f);
            }*/

            /*Build b2;
            //b.silent = true;
            b2.Local = false;
            //b2.knownTargets = knownTargets;
            b2.NamePrefix = p.ppath.slice(0, 2);
            //b2.load(dll);

            auto &c = b2.solutions[0].getChildren();
            for (auto &[pp, t] : c)
                t->solution = this;
            children.insert(c.begin(), c.end());
            copyChecksFrom(b2.solutions[0]);
            b2.solutions[0].children.clear();
            //}));
        }
        //waitAndGet(fs);

        // fix solution ptr
        //for (auto &c : children)
            //c.second->solution = this;*/

            //std::unordered_map<Package, Build> build_configs;
        while (!ud.empty())
        {
            auto rd = r.resolved_packages;
            for (auto &[porig, p] : rd)
            {
                for (auto &[n, t] : getChildren())
                {
                    if (p == t->pkg && ud[porig])
                    {
                        ud[porig]->target = (NativeTarget *)t.get();
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
            }));
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
                dptr->target = &const_cast<Solution*>(this)->getTarget<NativeTarget>(PackageId(r.value()));
                rm.insert(up);
            }
            else
            {
                for (const auto &[p,t] : getChildren())
                {
                    if (up.canBe(p))
                    {
                        dptr->target = (NativeTarget*)t.get();
                        rm.insert(up);
                        break;
                    }
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
    /*static */const auto host_os = detectOS();

    HostOS = host_os;
    Settings.TargetOS = HostOS; // temp

    languages = getLanguages();
    findCompiler();

    Settings.Native.ASMCompiler = ((ASMLanguage*)languages[LanguageType::ASM].get())->compiler;
    Settings.Native.CCompiler = ((CLanguage*)languages[LanguageType::C].get())->compiler;
    Settings.Native.CPPCompiler = ((CPPLanguage*)languages[LanguageType::CPP].get())->compiler;

    Settings.Native.CompilerType = Settings.Native.CPPCompiler->Type;
}

Build::~Build()
{
    // first destroy children as they might have data references to modules
    solutions.clear();

    // maybe also clear checks?
    // or are they solution-specific?

    getModuleStorage(base_ptr).modules.clear();
}

void Build::findCompiler()
{
    switch (HostOS.Type)
    {
    case OSType::Windows:
        if (
            !VisualStudio().findToolchain(*this) &&
            //!ClangCl::findToolchain(*this) &&
            //!Clang::findToolchain(*this) &&
            1
            )
        {
            throw std::runtime_error("Try to add more compilers");
        }
        //if (FileTransforms.IsEmpty())
        break;
    case OSType::Linux:
        if (
            !GNU().findToolchain(*this) &&
            //!ClangCl::findToolchain(*this) &&
            //!Clang::findToolchain(*this) &&
            1
            )
        {
            throw std::runtime_error("Try to add more compilers");
        }
        //if (FileTransforms.IsEmpty())
        break;
    }
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
        fs.push_back(e.push([&s] { s.performChecks(); }));
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
        fs.push_back(e.push([&s] { s.prepare(); }));
    waitAndGet(fs);

    if (!silent)
        LOG_INFO(logger, "Prepare time: " << t.getTimeFloat() << " s.");
}

Solution &Build::addSolution()
{
    return solutions.emplace_back(*this);
}

PackagePath Build::getSelfTargetName(const path &fn)
{
    return "loc.sw.self." + sha256_short(fn.string()) + "." + fn.stem().string();
}

SharedLibraryTarget &Build::createTarget(const path &fn)
{
    auto &solution = solutions[0];
    solution.IsConfig = true;
    auto &lib = solution.addTarget<SharedLibraryTarget>(getSelfTargetName(fn), "local");
    /*if (!lib.Local)
    {
        lib.BinaryDir = pkg.getDirObj() / "build" / getConfig(true) / "cfg";
        lib.init2(); // re-set output dirs
    }*/
    solution.IsConfig = false;
    return lib;
}

path Build::get_module_name(const path &fn)
{
    auto &lib = createTarget(fn);
    return lib.getOutputFile();
}

FilesMap Build::build_configs(const Files &files)
{
    /*SCOPE_EXIT
    {
        getFileStorage().reset();
    };*/

    if (solutions.empty())
        addSolution();

    auto &solution = solutions[0];

    //solution.Local = true;
    solution.Settings.Native.LibrariesType = LibraryType::Static;
#ifndef NDEBUG
    solution.Settings.Native.ConfigurationType = ConfigurationType::Debug;
#endif

#if defined(CPPAN_OS_WINDOWS)
    auto &implib = solution.getImportLibrary();
#endif

    check_self(solution.Checks);
    solution.performChecks();
    build_self(solution);

    auto prepare_config = [this,
#if defined(CPPAN_OS_WINDOWS)
            &implib,
#endif
            &solution
    ](const auto &fn, const auto &deps)
    {
        auto &lib = createTarget(fn);
#if defined(CPPAN_OS_WINDOWS)
        lib += implib;
#endif
        lib.AutoDetectOptions = false;
        lib.CPPVersion = CPPLanguageStandard::CPP17;
        if (auto L = lib.languages[LanguageType::CPP]->template as<CPPLanguage>())
        {
            if (auto C = L->compiler->template as<VisualStudioCompiler>())
            {
                //C->RuntimeLibrary() = RuntimeLibraryType::MultiThreadedDLL;
            }
        }
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

        if (auto s = lib[fn].template as<CPPSourceFile>())
        {
            if (auto c = sf->compiler->as<VisualStudioCompiler>())
            {
                c->ForcedIncludeFiles().push_back(p.header);
            }
            else if (auto c = sf->compiler->as<ClangClCompiler>())
            {
                c->ForcedIncludeFiles().push_back(p.header);
            }
            else if (auto c = sf->compiler->as<ClangCompiler>())
            {
                c->ForcedIncludeFiles().push_back(p.header);
            }
            else if (auto c = sf->compiler->as<GNUCompiler>())
            {
                c->ForcedIncludeFiles().push_back(p.header);
            }
        }

        for (auto &[k, v] : lib)
        {
            if (!v)
                continue;
            if (auto s = v->template as<CPPSourceFile>())
            {
#ifdef CPPAN_DEBUG
                if (auto C = s->compiler->template as<VisualStudioCompiler>())
                {
                    C->RuntimeLibrary = vs::RuntimeLibraryType::MultiThreadedDLLDebug;
                }
                else if (auto C = s->compiler->template as<GNUCompiler>())
                {
                    C->GenerateDebugInfo = true;
                }
#endif
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
        lib.Definitions["SW_PACKAGE_API"] = "extern \"C\"";
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

        lib += solution.getTarget<NativeTarget>("sw.manager");
        auto d = lib + solution.getTarget<NativeTarget>("sw.driver.cpp");
        d->IncludeDirectoriesOnly = true;
        //lib += solution.getTarget<NativeTarget>("org.sw.demo.boost.filesystem");
        lib += solution.getTarget<NativeTarget>("pub.egorpugin.primitives.command");
        lib += solution.getTarget<NativeTarget>("pub.egorpugin.primitives.hash");
        lib += solution.getTarget<NativeTarget>("pub.egorpugin.primitives.http");
        for (auto &d : deps)
            lib += std::make_shared<Dependency>(d);

        auto i = solution.children.find(lib.pkg);
        solution.TargetsToBuild[i->first] = i->second;

        return lib.getOutputFile();
    };

    FilesMap r;
    for (auto &fn : files)
    {
        UnresolvedPackages udeps = getFileDependencies(fn);

        auto f = read_file(fn);
        auto abs = fs::absolute(fn);
        auto o = getDirectories().storage_dir_tmp / "cfg" / sha256_short(abs.u8string());
        write_file_if_different(o += fn.extension(), f);



        static const std::regex r_pragma("^#pragma\\s+sw\\s+require\\s+(\\S+)(\\s+(\\S+))?");
        std::smatch m;
        String s;
        int line = 1;
        while (std::regex_search(f, m, r_pragma))
        {
            auto m1 = m[1].str();
            if (m1 == "header")
            {
                auto pref = m.prefix().str();
                line += std::count(pref.begin(), pref.end(), '\n');
                auto pkg = extractFromString(m1).resolve();
                auto h = getPackageHeader(pkg);
                auto deps = getFileDependencies(h);
                udeps.insert(deps.begin(), deps.end());
                s += pref +
                    "#line " + std::to_string(line) + " \"" + normalize_path(abs) + "\"\n" +
                    "#include \"" + normalize_path(h) + "\"";
                f = m.suffix().str();
            }
            else
                udeps.insert(extractFromString(m[3].str()));
        }
        s += f;
        auto o = getDirectories().storage_dir_tmp / "cfg" / sha256_short(abs.u8string());
        write_file(o += fn.extension(), s);

        r[abs] = prepare_config(o, udeps);
    }

    Solution::execute();

    return r;
}

path Build::build(const path &fn)
{
    // build in separate solution
    Build b;// (*this);
    //b.configure = configure;
    //b.Local = Local;
    return b.build_configs({ fn }).begin()->second;
}

void Build::build_and_load(const path &fn)
{
    auto dll = build(fn);
    load(dll);

    // why separate build? not this object
    /*Build b;
    b.configure = configure;
    b.Local = Local;
    b.load(dll);*/
}

bool Build::execute()
{
    try
    {
        Solution::execute();
        return true;
    }
    catch (std::exception &e) { LOG_ERROR(logger, "error during build: " << e.what()); }
    catch (...) {}
    return false;
}

void Build::build_and_run(const path &fn)
{
    auto dll = build(fn);
    //if (!fn.parent_path().empty())
        //fs::current_path(fn.parent_path());

    // why separate build? not this object
    /*Build b;
    b.configure = configure;
    b.Local = Local;
    b.load(dll);*/
    load(dll);

    if (!::sw::Settings::get_user_settings().generator.empty())
    {
        /*b.*/prepare();
        /*b.*/getCommands();

        Generator g;
        g.file = fn.filename();
        //g.dir = fs::current_path();
        g.generate(*this);
        return;
    }

    /*b.*/Solution::execute();
}

/*void Build::run(const path &dll)
{
    load(dll);
    execute();
}*/

void Build::build_package(const String &s)
{
    //auto [pkg,pkgs] = resolve_dependency(s);
    auto pkg = extractFromString(s);

    // resolve only deps needed
    Resolver r;
    r.resolve_dependencies({ pkg });
    auto dd = r.getDownloadDependencies();

    // if we met unknown package we'll fail
    // probably fix this
    // otherwise we will be downloading and building much more packages
    // TODO: choose behavior
    for (auto &p : dd)
        knownTargets.insert(p);

    if (solutions.empty())
        addSolution();

    // always try to take first path
    std::unordered_map<PackageVersionGroupNumber, std::set<path>> cfgs2;
    for (auto &[p, gn] : r.getDownloadDependenciesWithGroupNumbers())
        cfgs2[gn].insert(p.getDirSrc2() / getConfigFilename());

    Files cfgs;
    for (auto &[gn, s] : cfgs2)
        cfgs.insert(*s.begin());

    Build b;
    b.Local = false;
    auto dlls = b.build_configs(cfgs);

    getFileStorage().reset();

    // make parallel

    // gather checks
    for (auto &s : solutions)
    {
        s.Local = false;
        //s.PostponeFileResolving = true;
        s.knownTargets = knownTargets;

        // on the first step we load configure information
        for (auto &p : dd)
        {
            auto f = p.getDirSrc2() / getConfigFilename();
            if (cfgs.find(f) == cfgs.end())
                continue;
            auto dll = dlls[f];

            s.NamePrefix = p.ppath.slice(0, 2);
            getModuleStorage(base_ptr).get(dll).check(s.Checks);
        }
    }

    performChecks();

    // load build configs
    for (auto &s : solutions)
    {
        for (auto &p : dd)
        {
            auto f = p.getDirSrc2() / getConfigFilename();
            if (cfgs.find(f) == cfgs.end())
                continue;
            auto dll = dlls[f];

            s.NamePrefix = p.ppath.slice(0, 2);
            //ScopedValue<decltype(s.children)> c(s.children);
            getModuleStorage(base_ptr).get(dll).build(s);
        }
    }

    // fix dependencies to selected children
    /*for (auto &s : solutions)
    {
        for (auto &[pkg, t] : s.children)
        {
            auto nt = (NativeExecutedTarget*)t.get();
            nt->TargetOptionsGroup::iterate<WithoutSourceFileStorage, WithNativeOptions>([&s](auto &v, auto &gs)
            {
                for (auto &d : v.Dependencies)
                {
                    if ()
                }
            });
        }
    }*/

    // set what packages we build
    for (auto &s : solutions)
    {
        auto p = r.resolved_packages[pkg];
        s.TargetsToBuild[p] = s.getTargetPtr(p);
    }

    if (!::sw::Settings::get_user_settings().generator.empty())
    {
        prepare();
        getCommands();

        Generator g;
        g.file = s;
        //g.dir = fs::current_path();
        g.generate(*this);
        return;
    }

    execute();
}

void Build::load(const path &dll)
{
    if (configure)
        getModuleStorage(base_ptr).get(dll).configure(*this);

    if (solutions.empty())
        addSolution();

    // some packages want checks in their build body
    //if (perform_checks)
    {
        // make parallel?
        for (auto &s : solutions)
            getModuleStorage(base_ptr).get(dll).check(s.Checks);
        performChecks();
    }

    for (auto &s : solutions)
    {
        //ScopedValue<decltype(s.children)> c(s.children);
        getModuleStorage(base_ptr).get(dll).build(s);
    }
}

PackageDescriptionMap Build::getPackages() const
{
    PackageDescriptionMap m;
    if (!solutions.empty())
    {
        for (auto &[pkg, t] : solutions.begin()->children)
        {
            auto nt = (NativeExecutedTarget*)t.get();

            nlohmann::json j;

            // source, version, path
            save_source(j["source"], t->source);
            j["version"] = pkg.getVersion().toString();
            j["path"] = pkg.ppath.toString();

            // files
            // we do not use nt->gatherSourceFiles(); as it removes deleted files
            Files files;
            for (auto &f : nt->gatherAllFiles())
            {
                if (File(f).isGenerated())
                    continue;
                files.insert(f);
            }
            if (files.empty())
                throw std::runtime_error("No files found");
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
                nlohmann::json jd;
                jd["path"] = d->getPackage().ppath.toString();
                jd["versions"] = d->getPackage().range.toString();
                j["dependencies"].push_back(jd);
            }

            auto s = j.dump();
            m[pkg] = std::make_unique<JsonPackageDescription>(s);
        }
    }
    return m;
}

const Module &ModuleStorage::get(const path &dll)
{
    {
        std::shared_lock<std::shared_mutex> lk(m);
        auto i = modules.find(dll);
        if (i != modules.end())
            return i->second;
    }
    {
        std::unique_lock<std::shared_mutex> lk(m);
        return modules.emplace(dll, dll).first->second;
    }
}

Module::Module(const path &dll)
    : module(dll.wstring())
{
    if (module.has("build"))
        build_ = module.get<void(Solution&)>("build");
    if (module.has("check"))
        check = module.get<void(Checker&)>("check");
    if (module.has("configure"))
        configure = module.get<void(Solution&)>("configure");
}

void Module::build(Solution &s) const
{
    Solution s2(s);
    //build_(s2);
    build_(s);
    for (auto &[p, t] : s2.children)
    {
        if (s.knownTargets.find(p) == s.knownTargets.end())
            continue;
        s.add(t);
    }
}

ModuleStorage &getModuleStorage(Solution &owner)
{
    static std::map<void*, ModuleStorage> s;
    return s[&owner];
}

}
