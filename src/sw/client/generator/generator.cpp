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
#include <sw/support/filesystem.h>

#include <boost/algorithm/string.hpp>
#include <nlohmann/json.hpp>
#include <primitives/sw/cl.h>

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
    case GeneratorType::VisualStudioNMake:
        return "vs_nmake";
    case GeneratorType::VisualStudioUtility:
        return "vs_util";
    case GeneratorType::VisualStudioNMakeAndUtility:
        return "vs_nmake_util";
    case GeneratorType::Ninja:
        return "ninja";
    case GeneratorType::Batch:
        return "batch";
    case GeneratorType::Make:
        return "make";
    case GeneratorType::NMake:
        return "nmake";
    case GeneratorType::Shell:
        return "shell";
    case GeneratorType::CompilationDatabase:
        return "compdb";
    default:
        throw std::logic_error("not implemented");
    }
}

String toString(GeneratorType t)
{
    switch (t)
    {
    case GeneratorType::VisualStudio:
        return "Visual Studio";
    case GeneratorType::VisualStudioNMake:
        return "Visual Studio NMake";
    case GeneratorType::VisualStudioUtility:
        return "Visual Studio Utility";
    case GeneratorType::VisualStudioNMakeAndUtility:
        return "Visual Studio NMake and Utility";
    case GeneratorType::Ninja:
        return "Ninja";
    case GeneratorType::Batch:
        return "Batch";
    case GeneratorType::Make:
        return "Make";
    case GeneratorType::NMake:
        return "NMake";
    case GeneratorType::Shell:
        return "Shell";
    case GeneratorType::CompilationDatabase:
        return "CompDB";
    default:
        throw std::logic_error("not implemented");
    }
}

GeneratorType fromString(const String &s)
{
    // make icasecmp
    if (0)
        ;
    else if (boost::istarts_with(s, "VS_IDE") || boost::istarts_with(s, "VS"))
        return GeneratorType::VisualStudio;
    else if (boost::istarts_with(s, "VS_NMake"))
        return GeneratorType::VisualStudioNMake;
    else if (boost::istarts_with(s, "VS_Utility") || boost::istarts_with(s, "VS_Util"))
        return GeneratorType::VisualStudioUtility;
    else if (boost::istarts_with(s, "VS_NMakeAndUtility") || boost::istarts_with(s, "VS_NMakeAndUtil") || boost::istarts_with(s, "VS_NMakeUtil"))
        return GeneratorType::VisualStudioNMakeAndUtility;
    else if (boost::iequals(s, "Ninja"))
        return GeneratorType::Ninja;
    else if (boost::iequals(s, "Make") || boost::iequals(s, "Makefile"))
        return GeneratorType::Make;
    else if (boost::iequals(s, "NMake"))
        return GeneratorType::NMake;
    else if (boost::iequals(s, "Batch"))
        return GeneratorType::Batch;
    else if (boost::iequals(s, "Shell"))
        return GeneratorType::Shell;
    else if (boost::iequals(s, "CompDb"))
        return GeneratorType::CompilationDatabase;
    //else if (boost::iequals(s, "qtc"))
        //return GeneratorType::qtc;
    return GeneratorType::UnspecifiedGenerator;
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

    ProgramShortCutter()
        : sc_generated("SW_PROGRAM_GENERATED_")
    {}

    String getProgramName(const String &in, const builder::Command &c)
    {
        bool gen = File(c.getProgram(), c.swctx.getFileStorage()).isGeneratedAtAll();
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
        print_progs(sc_generated);
        ctx.emptyLines();
    }

private:
    ProgramShortCutter1 sc;
    ProgramShortCutter1 sc_generated;
};

std::unique_ptr<Generator> Generator::create(const String &s)
{
    auto t = fromString(s);
    std::unique_ptr<Generator> g;
    switch (t)
    {
    case GeneratorType::VisualStudio:
    case GeneratorType::VisualStudioNMake:
    case GeneratorType::VisualStudioUtility:
    case GeneratorType::VisualStudioNMakeAndUtility:
    {
        auto g1 = std::make_unique<VSGenerator>();
        g1->version = Version(vsVersionFromString(s));
        g = std::move(g1);
        break;
    }
    case GeneratorType::Ninja:
        g = std::make_unique<NinjaGenerator>();
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
    default:
        throw std::logic_error("not implemented");
    }
    g->type = t;
    return g;
}

struct NinjaEmitter : primitives::Emitter
{
    NinjaEmitter(const SwBuild &b, const path &dir)
    {
        const String commands_fn = "commands.ninja";
        addLine("include " + commands_fn);
        emptyLines(1);

        auto ep = b.getExecutionPlan();

        for (auto &c : ep.commands)
            addCommand(b, *c);

        primitives::Emitter ctx_progs;
        sc.printPrograms(ctx_progs, [](auto &ctx, auto &prog, auto &alias)
        {
            ctx.addLine(alias + " = " + prog);
        });
        write_file(dir / commands_fn, ctx_progs.getText());
    }

private:
    path dir;
    ProgramShortCutter sc;

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
        if (b.getContext().getHostOs().Type != OSType::Windows)
            quotes = false;

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
        path rsp_dir = dir / "rsp";
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
        addText("$" + sc.getProgramName(prepareString(b, getShortName(prog), true), c) + " ");

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

void NinjaGenerator::generate(const SwBuild &swctx)
{
    // https://ninja-build.org/manual.html#_writing_your_own_ninja_files

    const auto dir = path(SW_BINARY_DIR) / toPathString(type) / swctx.getHash();

    NinjaEmitter ctx(swctx, dir);
    write_file(dir / "build.ninja", ctx.getText());
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
            if (File(i, c.swctx.getFileStorage()).isGeneratedAtAll())
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

    const auto d = fs::absolute(path(SW_BINARY_DIR) / toPathString(type) / b.getHash());

    auto ep = b.getExecutionPlan();

    MakeEmitter ctx;
    ctx.nmake = type == GeneratorType::NMake;

    const String commands_fn = "commands.mk";
    ctx.clear();

    ctx.include(commands_fn);
    ctx.addLine();

    // all
    Files outputs;
    for (auto &c : ep.commands)
        outputs.insert(c->outputs.begin(), c->outputs.end());
    ctx.addTarget("all", outputs);

    // print commands
    for (auto &c : ep.commands)
        ctx.addCommand(*c, d);

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

void ShellGenerator::generate(const SwBuild &b)
{
    const auto d = path(SW_BINARY_DIR) / toPathString(type) / b.getHash();

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

    auto &ctx_progs = ctx.addEmitter();

    ProgramShortCutter sc;

    // print commands
    int i = 1;
    for (auto &c : ep.commands)
    {
        ctx.addLine("echo [" + std::to_string(i++) + "/" + std::to_string(ep.commands.size()) + "] " + c->getName());

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

    const auto d = path(SW_BINARY_DIR) / toPathString(type) / b.getHash();

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
