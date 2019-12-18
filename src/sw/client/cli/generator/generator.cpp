/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2019 Egor Pugin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "generator.h"

#include <sw/builder/file.h>
#include <sw/builder/execution_plan.h>
#include <sw/core/build.h>
#include <sw/core/sw_context.h>
#include <sw/manager/storage.h>
#include <sw/support/filesystem.h>

#include <boost/algorithm/string.hpp>
#include <nlohmann/json.hpp>
#include <primitives/sw/cl.h>
#include <primitives/pack.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "generator");

using namespace sw;

int vsVersionFromString(const String &s);

String toPathString(GeneratorType t)
{
    switch (t)
    {
    case GeneratorType::VisualStudio:
        return "vs";
    case GeneratorType::Ninja:
        return "ninja";
    case GeneratorType::Batch:
        return "batch";
    case GeneratorType::CMake:
        return "cmake";
    case GeneratorType::Make:
        return "make";
    case GeneratorType::NMake:
        return "nmake";
    case GeneratorType::Shell:
        return "shell";
    case GeneratorType::CompilationDatabase:
        return "compdb";
    case GeneratorType::SwExecutionPlan:
        return "swexplan";
    case GeneratorType::SwBuildDescription:
        return "swbdesc";
    case GeneratorType::RawBootstrapBuild:
        return "rawbootstrap";
    default:
        throw SW_LOGIC_ERROR("not implemented");
    }
}

String toPathString(VsGeneratorType t)
{
    switch (t)
    {
    case VsGeneratorType::VisualStudio:
        return "vs";
    case VsGeneratorType::VisualStudioNMake:
        return "vs_nmake";
    case VsGeneratorType::VisualStudioUtility:
        return "vs_util";
    case VsGeneratorType::VisualStudioNMakeAndUtility:
        return "vs_nmake_util";
    default:
        throw SW_LOGIC_ERROR("not implemented");
    }
}

static String toString(GeneratorType t)
{
    switch (t)
    {
    case GeneratorType::VisualStudio:
        return "Visual Studio";
    case GeneratorType::Ninja:
        return "Ninja";
    case GeneratorType::Batch:
        return "Batch";
    case GeneratorType::Make:
        return "Make";
    case GeneratorType::CMake:
        return "CMake";
    case GeneratorType::NMake:
        return "NMake";
    case GeneratorType::Shell:
        return "Shell";
    case GeneratorType::CompilationDatabase:
        return "CompDB";
    case GeneratorType::SwExecutionPlan:
        return "Sw Execution Plan";
    case GeneratorType::SwBuildDescription:
        return "Sw Build Description";
    case GeneratorType::RawBootstrapBuild:
        return "Raw Bootstrap Build";
    default:
        throw SW_LOGIC_ERROR("not implemented");
    }
}

static String toString(VsGeneratorType t)
{
    switch (t)
    {
    case VsGeneratorType::VisualStudio:
        return "Visual Studio";
    case VsGeneratorType::VisualStudioNMake:
        return "Visual Studio NMake";
    case VsGeneratorType::VisualStudioUtility:
        return "Visual Studio Utility";
    case VsGeneratorType::VisualStudioNMakeAndUtility:
        return "Visual Studio NMake and Utility";
    default:
        throw SW_LOGIC_ERROR("not implemented");
    }
}

static GeneratorType fromString(const String &s)
{
    // make icasecmp
    if (0)
        ;
    else if (boost::istarts_with(s, "VS_IDE") || boost::istarts_with(s, "VS"))
        return GeneratorType::VisualStudio;
    else if (boost::iequals(s, "Ninja"))
        return GeneratorType::Ninja;
    else if (boost::iequals(s, "Make") || boost::iequals(s, "Makefile"))
        return GeneratorType::Make;
    else if (boost::iequals(s, "CMake"))
        return GeneratorType::CMake;
    else if (boost::iequals(s, "NMake"))
        return GeneratorType::NMake;
    else if (boost::iequals(s, "Batch"))
        return GeneratorType::Batch;
    else if (boost::iequals(s, "Shell"))
        return GeneratorType::Shell;
    else if (boost::iequals(s, "CompDb"))
        return GeneratorType::CompilationDatabase;
    else if (boost::iequals(s, "SwExPlan"))
        return GeneratorType::SwExecutionPlan;
    else if (boost::iequals(s, "SwBDesc"))
        return GeneratorType::SwBuildDescription;
    else if (boost::iequals(s, "raw-bootstrap"))
        return GeneratorType::RawBootstrapBuild;
    //else if (boost::iequals(s, "qtc"))
    //return GeneratorType::qtc;
    throw SW_RUNTIME_ERROR("Unknown generator: " + s);
}

static VsGeneratorType fromStringVs(const String &s)
{
    // make icasecmp
    if (0)
        ;
    else if (boost::istarts_with(s, "VS_IDE") || boost::istarts_with(s, "VS"))
        return VsGeneratorType::VisualStudio;
    else if (boost::istarts_with(s, "VS_NMake"))
        return VsGeneratorType::VisualStudioNMake;
    else if (boost::istarts_with(s, "VS_Utility") || boost::istarts_with(s, "VS_Util"))
        return VsGeneratorType::VisualStudioUtility;
    else if (boost::istarts_with(s, "VS_NMakeAndUtility") || boost::istarts_with(s, "VS_NMakeAndUtil") || boost::istarts_with(s, "VS_NMakeUtil"))
        return VsGeneratorType::VisualStudioNMakeAndUtility;
    throw SW_RUNTIME_ERROR("Unknown generator: " + s);
}

std::unique_ptr<Generator> Generator::create(const String &s)
{
    auto t = fromString(s);
    std::unique_ptr<Generator> g;
    switch (t)
    {
    case GeneratorType::VisualStudio:
    {
        auto g1 = std::make_unique<VSGenerator>();
        //g1->vs_version = Version(vsVersionFromString(s));
        g1->vstype = fromStringVs(s);
        g = std::move(g1);
        break;
    }
    case GeneratorType::Ninja:
        g = std::make_unique<NinjaGenerator>();
        break;
    case GeneratorType::CMake:
        g = std::make_unique<CMakeGenerator>();
        break;
    case GeneratorType::NMake:
    case GeneratorType::Make:
        g = std::make_unique<MakeGenerator>();
        break;
    case GeneratorType::Batch:
    {
        auto g1 = std::make_unique<ShellGenerator>();
        g1->batch = true;
        g = std::move(g1);
        break;
    }
    case GeneratorType::Shell:
        g = std::make_unique<ShellGenerator>();
        break;
    case GeneratorType::CompilationDatabase:
        g = std::make_unique<CompilationDatabaseGenerator>();
        break;
    case GeneratorType::SwExecutionPlan:
        g = std::make_unique<SwExecutionPlanGenerator>();
        break;
    case GeneratorType::SwBuildDescription:
        g = std::make_unique<SwBuildDescriptionGenerator>();
        break;
    case GeneratorType::RawBootstrapBuild:
        g = std::make_unique<RawBootstrapBuildGenerator>();
        break;
    default:
        SW_UNIMPLEMENTED;
    }
    g->type = t;
    return g;
}

path Generator::getRootDirectory(const sw::SwBuild &b) const
{
    return fs::current_path() / path(SW_BINARY_DIR) / "g" / toPathString(getType()) / b.getHash();
}

struct ProgramShortCutter1
{
    struct iter
    {
        ProgramShortCutter1 &sc;
        size_t i;

        bool operator!=(const iter &rhs) const { return i != rhs.i; }
        auto operator*() { return sc.programs.find(sc.nprograms[i]); }

        iter &operator++()
        {
            i++;
            return *this;
        }
    };

    ProgramShortCutter1(const String &prefix = "SW_PROGRAM_")
        : prefix(prefix)
    {}

    String getProgramName(const String &in)
    {
        if (programs[in].empty())
        {
            programs[in] = prefix + std::to_string(programs.size());
            nprograms[programs.size()] = in;
        }
        return programs[in];
    }

    bool empty() const { return programs.empty(); }

    auto begin() const { return iter{ (ProgramShortCutter1 &)*this, 1 }; }
    auto end() const { return iter{ (ProgramShortCutter1 &)*this, programs.size() + 1 }; }

private:
    String prefix;
    std::map<String, String> programs;
    std::map<size_t, String> nprograms;
};

struct ProgramShortCutter
{
    //                                                           program           alias
    using F = std::function<void(primitives::Emitter &ctx, const String &, const String &)>;

    ProgramShortCutter(bool print_sc_generated = false)
        : sc_generated("SW_PROGRAM_GENERATED_")
        , print_sc_generated(print_sc_generated)
    {}

    String getProgramName(const String &in, const builder::Command &c, bool *untouched = nullptr)
    {
        bool gen = File(c.getProgram(), c.getContext().getFileStorage()).isGeneratedAtAll();
        if (gen && !print_sc_generated)
        {
            if (untouched)
                *untouched = true;
            return in;
        }
        if (untouched)
            *untouched = false;
        auto &progs = gen ? sc_generated : sc;
        return progs.getProgramName(in);
    }

    void printPrograms(primitives::Emitter &ctx, F f) const
    {
        auto print_progs = [&ctx, &f](const auto &a)
        {
            for (const auto &kv : a)
                f(ctx, kv->first, kv->second);
        };

        // print programs
        print_progs(sc);
        ctx.emptyLines();
        if (print_sc_generated)
            print_progs(sc_generated);
        ctx.emptyLines();
    }

private:
    ProgramShortCutter1 sc;
    ProgramShortCutter1 sc_generated;
    bool print_sc_generated;
};

struct NinjaEmitter : primitives::Emitter
{
    NinjaEmitter(const SwBuild &b, const path &dir)
        : dir(dir)
    {
        addLine("include " + commands_fn);
        emptyLines(1);

        auto ep = b.getExecutionPlan();

        for (auto &c : ep.getCommands())
            addCommand(b, *static_cast<builder::Command*>(c));

        primitives::Emitter ctx_progs;
        sc.printPrograms(ctx_progs, [](auto &ctx, auto &prog, auto &alias)
        {
            ctx.addLine(alias + " = " + prog);
        });
        write_file(dir / commands_fn, ctx_progs.getText());
    }

    Files getCreatedFiles() const
    {
        Files files;
        files.insert(dir / commands_fn);
        files.insert(getRspDir());
        return files;
    }

private:
    path dir;
    ProgramShortCutter sc;
    static inline const String commands_fn = "commands.ninja";

    path getRspDir() const
    {
        return dir / "rsp";
    }

    String getShortName(const path &p)
    {
#ifdef _WIN32
        std::wstring buf(4096, 0);
        path p2 = normalize_path_windows(p);
        if (!GetShortPathName(p2.wstring().c_str(), buf.data(), buf.size()))
            //throw SW_RUNTIME_ERROR("GetShortPathName failed for path: " + p.u8string());
            return normalize_path(p);
        return normalize_path(to_string(buf));
#else
        return normalize_path(p);
#endif
    }

    String prepareString(const SwBuild &b, const String &s, bool quotes = false)
    {
        //if (b.getContext().getHostOs().Type != OSType::Windows)
            //quotes = false;

        auto s2 = s;
        boost::replace_all(s2, ":", "$:");
        boost::replace_all(s2, "\"", "\\\"");
        if (quotes)
            return "\"" + s2 + "\"";
        return s2;
    }

    void addCommand(const SwBuild &b, const builder::Command &c)
    {
        bool rsp = c.needsResponseFile();
        if (b.getContext().getHostOs().Type == OSType::Windows)
            rsp = c.needsResponseFile(8000);
        path rsp_dir = getRspDir();
        path rsp_file = fs::absolute(rsp_dir / (std::to_string(c.getHash()) + ".rsp"));
        if (rsp)
            fs::create_directories(rsp_dir);

        auto has_mmd = false;
        auto prog = c.getProgram();

        addLine("rule c" + std::to_string(c.getHash()));
        increaseIndent();
        addLine("description = " + c.getName());
        addLine("command = ");
        if (b.getContext().getHostOs().Type == OSType::Windows)
        {
            addText("cmd /S /C ");
            addText("\"");
        }
        //else
        //addText("bash -c ");

        // env
        for (auto &[k, v] : c.environment)
        {
            if (b.getContext().getHostOs().Type == OSType::Windows)
                addText("set ");
            addText(k + "=" + v + " ");
            if (b.getContext().getHostOs().Type == OSType::Windows)
                addText("&& ");
        }

        // wdir
        if (!c.working_directory.empty())
        {
            addText("cd ");
            if (b.getContext().getHostOs().Type == OSType::Windows)
                addText("/D ");
            addText(prepareString(b, getShortName(c.working_directory), true) + " && ");
        }

        // prog
        bool untouched = false;
        auto progn = sc.getProgramName(prepareString(b, getShortName(prog), true), c, &untouched);
        addText((untouched ? "" : "$") + progn + " ");

        // args
        if (!rsp)
        {
            int i = 0;
            for (auto &a : c.arguments)
            {
                // skip exe
                if (!i++)
                    continue;
                addText(prepareString(b, a->toString(), true) + " ");
                has_mmd |= "-MMD" == a->toString();
            }
        }
        else
        {
            addText("@" + rsp_file.u8string() + " ");
        }

        // redirections
        if (!c.in.file.empty())
            addText("< " + prepareString(b, getShortName(c.in.file), true) + " ");
        if (!c.out.file.empty())
            addText("> " + prepareString(b, getShortName(c.out.file), true) + " ");
        if (!c.err.file.empty())
            addText("2> " + prepareString(b, getShortName(c.err.file), true) + " ");

        //
        if (b.getContext().getHostOs().Type == OSType::Windows)
            addText("\"");
        if (prog.find("cl.exe") != prog.npos)
            addLine("deps = msvc");
        else if (has_mmd)
            addLine("depfile = " + (c.outputs.begin()->parent_path() / (c.outputs.begin()->stem().string() + ".d")).u8string());
        if (rsp)
        {
            addLine("rspfile = " + rsp_file.u8string());
            addLine("rspfile_content = ");
            int i = 0;
            for (auto &a : c.arguments)
            {
                // skip exe
                if (!i++)
                    continue;
                addText(prepareString(b, a->toString(), c.protect_args_with_quotes) + " ");
            }
        }
        decreaseIndent();
        addLine();

        addLine("build ");
        for (auto &o : c.outputs)
            addText(prepareString(b, getShortName(o)) + " ");
        //for (auto &o : c.intermediate)
            //addText(prepareString(b, getShortName(o)) + " ");
        addText(": c" + std::to_string(c.getHash()) + " ");
        for (auto &i : c.inputs)
            addText(prepareString(b, getShortName(i)) + " ");
        addLine();
    }
};

static Files generate_ninja(const SwBuild &b, const path &root_dir)
{
    // https://ninja-build.org/manual.html#_writing_your_own_ninja_files

    NinjaEmitter ctx(b, root_dir);
    write_file(root_dir / "build.ninja", ctx.getText());

    auto files = ctx.getCreatedFiles();
    files.insert(root_dir / "build.ninja");
    return files;
}

void NinjaGenerator::generate(const SwBuild &b)
{
    generate_ninja(b, getRootDirectory(b));
}

static bool should_print(const String &o)
{
    return o.find("showIncludes") == o.npos;
}

struct MakeEmitter : primitives::Emitter
{
    bool nmake = false;
    ProgramShortCutter sc;

    MakeEmitter()
        : Emitter("\t")
    {}

    void addKeyValue(const String &key, const String &value)
    {
        addLine(key + " = " + value);
    }

    void addKeyValue(const String &key, const path &value)
    {
        addKeyValue(key, "\"" + normalize_path(value) + "\"");
    }

    void include(const path &fn)
    {
        addLine("include " + normalize_path(fn));
    }

    void addComment(const String &s)
    {
        addLine("# " + s);
    }

    void addCommand(const String &command)
    {
        increaseIndent();
        addLine(command);
        decreaseIndent();
    }

    void addCommands(const String &name, const Strings &commands)
    {
        addCommand("@echo " + name);
        addCommands(commands);
    }

    void addCommands(const Strings &commands = {})
    {
        for (auto &c : commands)
            addCommand(c);
    }

    void addTarget(const String &name, const Files &inputs, const Strings &commands = {})
    {
        addLine(name + " : ");
        addText(printFiles(inputs));
        //          name,
        addCommands(commands);
        addLine();
    }

    void addCommand(const builder::Command &c, const path &d)
    {
        std::stringstream stream;
        stream << std::hex << c.getHash();
        std::string result(stream.str());

        auto rsp = d / "rsp" / c.getResponseFilename();

        addComment(c.getName() + ", hash = 0x" + result);

        addLine(printFiles(c.outputs));
        addText(" : ");
        //addText(printFiles(c.inputs));
        for (auto &i : c.inputs)
        {
            if (File(i, c.getContext().getFileStorage()).isGeneratedAtAll())
            {
                addText(printFile(i));
                addText(" ");
            }
        }
//         if (c.needsResponseFile())
//         {
//             addText(" ");
//             addText(printFile(rsp));
//         }

        Strings commands;
        commands.push_back(mkdir(c.getGeneratedDirs(), true));

        String s;
        s += "@";
        if (!c.working_directory.empty())
            s += "cd \"" + normalize_path(c.working_directory) + "\" && ";

        for (auto &[k, v] : c.environment)
        {
            if (nmake)
                s += "set ";
            s += k + "=" + v;
            if (nmake)
                s += "\n@";
            else
                s += " \\";
        }

        auto prog = c.getProgram();
        s += "$(" + sc.getProgramName("\"" + prog + "\"", c) + ") ";

        if (!c.needsResponseFile())
        {
            int i = 0;
            for (auto &a : c.arguments)
            {
                // skip exe
                if (!i++)
                    continue;
                if (should_print(a->toString()))
                    s += a->quote() + " ";
            }
            s.resize(s.size() - 1);
        }
        else
            s += "@" + normalize_path(rsp);

        if (!c.in.file.empty())
            s += " < " + normalize_path(c.in.file);
        if (!c.out.file.empty())
            s += " > " + normalize_path(c.out.file);
        if (!c.err.file.empty())
            s += " 2> " + normalize_path(c.err.file);

        // end of command
        commands.push_back(s);

        addCommands(c.getName(), commands);
        addLine();

        if (c.needsResponseFile())
        {
            write_file_if_different(rsp, c.getResponseFileContents(false));

//             commands.clear();
//
//             auto rsps = normalize_path(rsp);
//             commands.push_back(mkdir({ rsp.parent_path() }, true));
//             commands.push_back("@echo > " + rsps);
//             for (auto &a : c.args)
//             {
//                 if (should_print(a))
//                     commands.push_back("@echo \"\\\"" + a + "\\\"\" >> " + rsps);
//             }
//
//             addTarget(normalize_path(rsp), {}, commands);
        }
    }

    static String printFiles(const Files &inputs, bool quotes = false)
    {
        String s;
        for (auto &f : inputs)
        {
            s += printFile(f, quotes);
            s += " ";
        }
        if (!s.empty())
            s.resize(s.size() - 1);
        return s;
    }

    static String printFile(const path &p, bool quotes = false)
    {
        String s;
        if (quotes)
            s += "\"";
        s += normalize_path(p);
        if (!quotes)
            boost::replace_all(s, " ", "\\\\ ");
        if (quotes)
            s += "\"";
        return s;
    }

    String mkdir(const Files &p, bool gen = false)
    {
        if (nmake)
            return "@-if not exist " + normalize_path_windows(printFiles(p, gen)) + " mkdir " + normalize_path_windows(printFiles(p, gen));
        return "@-mkdir -p " + printFiles(p, gen);
    }
};

void MakeGenerator::generate(const SwBuild &b)
{
    // https://www.gnu.org/software/make/manual/html_node/index.html
    // https://en.wikipedia.org/wiki/Make_(software)

    const auto d = getRootDirectory(b);

    auto ep = b.getExecutionPlan();

    MakeEmitter ctx;
    ctx.nmake = getType() == GeneratorType::NMake;

    const String commands_fn = "commands.mk";
    ctx.clear();

    ctx.include(commands_fn);
    ctx.addLine();

    // all
    Files outputs;
    for (auto &c : ep.getCommands())
        outputs.insert(static_cast<builder::Command*>(c)->outputs.begin(), static_cast<builder::Command*>(c)->outputs.end());
    ctx.addTarget("all", outputs);

    // print commands
    for (auto &c : ep.getCommands())
        ctx.addCommand(*static_cast<builder::Command*>(c), d);

    // clean
    if (ctx.nmake)
        ctx.addTarget("clean", {}, { "@del " + normalize_path_windows(MakeEmitter::printFiles(outputs, true)) });
    else
        ctx.addTarget("clean", {}, { "@rm -f " + MakeEmitter::printFiles(outputs, true) });

    write_file(d / "Makefile", ctx.getText());
    ctx.clear();
    ctx.sc.printPrograms(ctx, [](auto &ctx, auto &prog, auto &alias)
    {
        ctx.addLine(alias + " = " + prog);
    });
    write_file(d / commands_fn, ctx.getText());
}

void CMakeGenerator::generate(const sw::SwBuild &b)
{
    SW_UNIMPLEMENTED;

    LOG_WARN(logger, "CMake generator is very experimental and subtle.");

    const auto d = getRootDirectory(b);

    auto ep = b.getExecutionPlan();

    primitives::Emitter ctx;

    ctx.addLine("cmake_minimum_required(VERSION 3.12.0)");
    ctx.addLine("project("s + "x" + " ASM C CXX)");

    for (auto &[pkg, tgts] : b.getTargetsToBuild())
    {
        if (tgts.empty())
        {
            continue;
            //throw SW_RUNTIME_ERROR("No targets in " + pkg.toString());
        }
        // filter out predefined targets
        if (b.getContext().getPredefinedTargets().find(pkg) != b.getContext().getPredefinedTargets().end())
            continue;

        auto &t = **tgts.begin();
        const auto &s = t.getInterfaceSettings();

        if (s["type"] == "native_executable")
            ctx.addLine("add_executable(" + pkg.toString() + ")");
        else
        {
            ctx.addLine("add_library(" + pkg.toString() + " ");

            // tgt
            auto st = "STATIC";
            if (s["type"] == "native_static_library")
                ;
            else if (s["type"] == "native_shared_library")
                st = "SHARED";
            if (s["header_only"] == "true")
                st = "INTERFACE";
            ctx.addText(st + ")"s);
        }
    }

    write_file(d / "CMakeLists.txt", ctx.getText());
}

void ShellGenerator::generate(const SwBuild &b)
{
    const auto d = getRootDirectory(b);

    auto ep = b.getExecutionPlan();

    primitives::Emitter ctx;

    if (batch)
    {
        ctx.addLine("@echo off");
        ctx.addLine("setlocal");
    }
    else
    {
        ctx.addLine("#!/bin/bash");
    }
    ctx.addLine();

    auto &ctx_progs = ctx.createInlineEmitter();

    ProgramShortCutter sc;

    // print commands
    int i = 1;
    for (auto &c1 : ep.getCommands())
    {
        auto c = static_cast<builder::Command *>(c1);
        ctx.addLine("echo [" + std::to_string(i++) + "/" + std::to_string(ep.getCommands().size()) + "] " + c->getName());

        // set new line
        ctx.addLine();

        // wdir
        if (!c->working_directory.empty())
            ctx.addText("cd \"" + normalize_path(c->working_directory) + "\" && ");

        // env
        for (auto &[k, v] : c->environment)
        {
            if (batch)
                ctx.addText("set ");
            ctx.addText(k + "=" + v + " ");
            if (batch)
                ctx.addText("&& ");
        }

        if (!c->needsResponseFile())
        {
            ctx.addText(batch ? "%" : "$");
            ctx.addText(sc.getProgramName(c->getProgram(), *c));
            if (batch)
                ctx.addText("%");
            ctx.addText(" ");
            int i = 0;
            for (auto &a : c->arguments)
            {
                // skip exe
                if (!i++)
                    continue;
                if (should_print(a->toString()))
                    ctx.addText(a->quote() + " ");
            }

            if (!c->in.file.empty())
                ctx.addText(" < " + normalize_path(c->in.file));
            if (!c->out.file.empty())
                ctx.addText(" > " + normalize_path(c->out.file));
            if (!c->err.file.empty())
                ctx.addText(" 2> " + normalize_path(c->err.file));
        }
        else
        {
            ctx.addLine("echo. 2> response.rsp");
            for (auto &a : c->arguments)
            {
                if (should_print(a->toString()))
                    ctx.addLine("echo " + a->quote() + " >> response.rsp");
            }
            ctx.addText(batch ? "%" : "$");
            ctx.addLine(sc.getProgramName(c->getProgram(), *c));
            if (batch)
                ctx.addText("%");
            ctx.addLine(" @response.rsp");
        }
        ctx.emptyLines(1);
    }

    sc.printPrograms(ctx_progs, [this](auto &ctx, auto &prog, auto &alias)
    {
        if (batch)
            ctx.addLine("set ");
        ctx.addLine(alias + "=\"" + normalize_path(prog) + "\"");
    });

    write_file(d / ("commands"s + (batch ? ".bat" : ".sh")), ctx.getText());
}

void CompilationDatabaseGenerator::generate(const SwBuild &b)
{
    static const std::set<String> exts{
        ".c", ".cpp", ".cxx", ".c++", ".cc", ".CPP", ".C++", ".CXX", ".C", ".CC"
    };

    const auto d = getRootDirectory(b);

    auto p = b.getExecutionPlan();

    nlohmann::json j;
    for (auto &[p, tgts] : b.getTargetsToBuild())
    {
        for (auto &tgt : tgts)
        {
            for (auto &c : tgt->getCommands())
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
                j2["arguments"].push_back(normalize_path(c->getProgram()));
                for (auto &a : c->arguments)
                    j2["arguments"].push_back(a->toString());
                j.push_back(j2);
            }
        }
    }
    write_file(d / "compile_commands.json", j.dump(2));
}

void SwExecutionPlanGenerator::generate(const sw::SwBuild &b)
{
    const auto d = getRootDirectory(b);
    auto fn = path(d) += ".explan";
    fs::create_directories(d.parent_path());

    auto ep = b.getExecutionPlan();
    ep.save(fn);
}

void SwBuildDescriptionGenerator::generate(const sw::SwBuild &b)
{
    const auto d = getRootDirectory(b);
    auto fn = path(d) += ".json";
    fs::create_directories(d.parent_path());

    nlohmann::json j;
    for (auto &[pkg, tgts] : b.getTargets())
    {
        if (tgts.empty())
        {
            continue;
            //throw SW_RUNTIME_ERROR("No targets in " + pkg.toString());
        }
        // filter out predefined targets
        if (b.getContext().getPredefinedTargets().find(pkg) != b.getContext().getPredefinedTargets().end())
            continue;

        for (auto &t : tgts)
        {
            nlohmann::json j1;
            // rename to settings?
            j1["key"] = nlohmann::json::parse(t->getSettings().toString());
            j1["value"] = nlohmann::json::parse(t->getInterfaceSettings().toString());
            j[pkg.toString()].push_back(j1);
        }
    }
    write_file(fn, j.dump(4));
}

void RawBootstrapBuildGenerator::generate(const sw::SwBuild &b)
{
    // bootstrap build is:
    //  1. ninja rules
    //  2. list of all used files except system ones

    auto dir = getRootDirectory(b);
    // remove hash part
    // this is very specific generator, so remove it for now
    // if users report to turn it back, turn it back
    dir = dir.parent_path();

    LOG_INFO(logger, "Generating ninja script");

    auto files = generate_ninja(b, dir);

    LOG_INFO(logger, "Building project");

    auto &mb = (SwBuild &)b;
    auto ep = mb.getExecutionPlan(); // save our commands
    mb.build(); // now build to get implicit inputs

    // gather files (inputs + implicit inputs)
    LOG_INFO(logger, "Gathering files");
    files.reserve(10000);
    for (auto &c1 : ep.getCommands())
    {
        auto &c = dynamic_cast<const sw::builder::Command &>(*c1);
        files.insert(c.inputs.begin(), c.inputs.end());
        files.insert(c.implicit_inputs.begin(), c.implicit_inputs.end());
    }

    LOG_INFO(logger, "Filtering files");

    const auto cp = fs::current_path();
    const auto sd = b.getContext().getLocalStorage().storage_dir;

    // filter out files not in current dir and not in storage
    std::unordered_map<path /* real file */, path /* path in archive */> files2;
    std::set<path> files_ordered;
    for (auto &f : files)
    {
        if (File(f, b.getContext().getFileStorage()).isGenerated())
            continue;
        if (is_under_root(f, sd))
        {
            files2[f] = f;
            files_ordered.insert(f);
        }
        else if (is_under_root(f, cp))
        {
            files2[f] = f;
            files_ordered.insert(f);
        }
    }

    String s;
    for (auto &f : files_ordered)
        s += normalize_path(f) + "\n";
    // remove last \n?
    write_file(dir / "files.txt", s);

    LOG_INFO(logger, "Packing files");

    auto bat = b.getContext().getHostOs().Type == OSType::Windows;
    String script;
    path script_fn = "bootstrap";
    if (bat)
        script_fn += ".bat";
    else
        script_fn += ".sh";
    if (bat)
        script += "@setlocal\n";
    script += "cd \"" + normalize_path(fs::current_path()) + "\"\n";
    script += "ninja -C \"" + normalize_path(dir) + "\"\n";
    write_file(script_fn, script);

    pack_files("bootstrap.tar.xz", files2);
}
