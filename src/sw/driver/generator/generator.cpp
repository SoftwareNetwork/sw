// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "generator.h"
#include "context.h"

#include "sw/driver/command.h"
#include "sw/driver/compiler.h"
#include "sw/driver/compiler_helpers.h"
#include "sw/driver/solution_build.h"
#include "sw/driver/target/native.h"

#include <sw/builder/execution_plan.h>
#include <sw/support/filesystem.h>

#include <primitives/sw/cl.h>
#include <primitives/win32helpers.h>

#include <boost/algorithm/string.hpp>
#include <nlohmann/json.hpp>

#include <sstream>
#include <stack>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "generator");

namespace sw
{

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

struct NinjaEmitter : primitives::Emitter
{
    void addCommand(const Build &b, const path &dir, const builder::Command &c)
    {
        String command;

        auto prog = c.getProgram().u8string();
        if (prog == "ExecuteCommand")
            return;

        bool rsp = c.needsResponseFile();
        path rsp_dir = dir / "rsp";
        path rsp_file = fs::absolute(rsp_dir / (std::to_string(c.getHash()) + ".rsp"));
        if (rsp)
            fs::create_directories(rsp_dir);

        auto has_mmd = false;

        addLine("rule c" + std::to_string(c.getHash()));
        increaseIndent();
        addLine("description = " + c.getName());
        addLine("command = ");
        if (b.Settings.TargetOS.Type == OSType::Windows)
        {
            addText("cmd /S /C ");
            addText("\"");
        }
        //else
            //addText("bash -c ");
        for (auto &[k, v] : c.environment)
        {
            if (b.Settings.TargetOS.Type == OSType::Windows)
                addText("set ");
            addText(k + "=" + v + " ");
            if (b.Settings.TargetOS.Type == OSType::Windows)
                addText("&& ");
        }
        if (!c.working_directory.empty())
        {
            addText("cd ");
            if (b.Settings.TargetOS.Type == OSType::Windows)
                addText("/D ");
            addText(prepareString(b, getShortName(c.working_directory), true) + " && ");
        }
        addText(prepareString(b, getShortName(prog), true) + " ");
        if (!rsp)
        {
            for (auto &a : c.args)
            {
                addText(prepareString(b, a, true) + " ");
                has_mmd |= "-MMD" == a;
            }
        }
        else
        {
            addText("@" + rsp_file.u8string() + " ");
        }
        if (!c.in.file.empty())
            addText("< " + prepareString(b, getShortName(c.in.file), true) + " ");
        if (!c.out.file.empty())
            addText("> " + prepareString(b, getShortName(c.out.file), true) + " ");
        if (!c.err.file.empty())
            addText("2> " + prepareString(b, getShortName(c.err.file), true) + " ");
        if (b.Settings.TargetOS.Type == OSType::Windows)
            addText("\"");
        if (prog.find("cl.exe") != prog.npos)
            addLine("deps = msvc");
        if (b.Settings.Native.CompilerType == CompilerType::GCC && has_mmd)
            addLine("depfile = " + (c.outputs.begin()->parent_path() / (c.outputs.begin()->stem().string() + ".d")).u8string());
        if (rsp)
        {
            addLine("rspfile = " + rsp_file.u8string());
            addLine("rspfile_content = ");
            for (auto &a : c.args)
                addText(prepareString(b, a, c.protect_args_with_quotes) + " ");
        }
        decreaseIndent();
        addLine();

        addLine("build ");
        for (auto &o : c.outputs)
            addText(prepareString(b, getShortName(o)) + " ");
        for (auto &o : c.intermediate)
            addText(prepareString(b, getShortName(o)) + " ");
        addText(": c" + std::to_string(c.getHash()) + " ");
        for (auto &i : c.inputs)
            addText(prepareString(b, getShortName(i)) + " ");
        addLine();
    }

private:
    String getShortName(const path &p)
    {
#ifdef _WIN32
        std::wstring buf(4096, 0);
        path p2 = normalize_path_windows(p);
        if (!GetShortPathName(p2.wstring().c_str(), buf.data(), buf.size()))
            //throw SW_RUNTIME_ERROR("GetShortPathName failed for path: " + p.u8string());
            return p.u8string();
        return to_string(buf);
#else
        return p.u8string();
#endif
    }

    String prepareString(const Build &b, const String &s, bool quotes = false)
    {
        if (b.Settings.TargetOS.Type != OSType::Windows)
            quotes = false;

        auto s2 = s;
        boost::replace_all(s2, ":", "$:");
        boost::replace_all(s2, "\"", "\\\"");
        if (quotes)
            return "\"" + s2 + "\"";
        return s2;
    }
};

void NinjaGenerator::generate(const Build &b)
{
    // https://ninja-build.org/manual.html#_writing_your_own_ninja_files

    const auto dir = path(SW_BINARY_DIR) / toPathString(type) / b.solutions[0].getConfig();

    NinjaEmitter ctx;

    auto ep = b.getExecutionPlan();
    for (auto &c : ep.commands)
        ctx.addCommand(b, dir, *c);

    auto t = ctx.getText();
    //if (b.Settings.TargetOS.Type != OSType::Windows)
        //std::replace(t.begin(), t.end(), '\\', '/');

    write_file(dir / "build.ninja", t);
}

struct MakeEmitter : primitives::Emitter
{
    bool nmake = false;
    std::unordered_map<path, size_t> programs;
    std::unordered_map<path, size_t> generated_programs;

    MakeEmitter()
        : Emitter("\t")
    {}

    void gatherPrograms(const Solution::CommandExecutionPlan::Vec &commands)
    {
        // gather programs
        for (auto &c : commands)
        {
            auto prog = c->getProgram();
            auto &progs = File(prog, *c->fs).isGeneratedAtAll() ? generated_programs : programs;

            auto n = progs.size() + 1;
            if (progs.find(prog) == progs.end())
                progs[prog] = n;
        }

        auto print_progs = [this](auto &a, bool gen = false)
        {
            std::map<int, path> r;
            for (auto &[k, v] : a)
                r[v] = k;
            for (auto &[v, k] : r)
                addKeyValue(program_name(v, gen), k);
        };

        // print programs
        print_progs(programs);
        addLine();
        print_progs(generated_programs, true);
    }

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
        addCommands(/* name, */ commands);
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
            if (File(i, *c.fs).isGeneratedAtAll())
            {
                addText(printFile(i));
                addText(" ");
            }
        }
        /*if (c.needsResponseFile())
        {
            addText(" ");
            addText(printFile(rsp));
        }*/

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
        bool gen = File(prog, *c.fs).isGeneratedAtAll();
        auto &progs = gen ? generated_programs : programs;
        s += "$(" + program_name(progs[prog], gen) + ") ";

        if (!c.needsResponseFile())
        {
            for (auto &a : c.args)
            {
                if (should_print(a))
                    s +=
                    "\"" +
                    a
                    + "\""
                    + " "
                    ;
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

            /*commands.clear();

            auto rsps = normalize_path(rsp);
            commands.push_back(mkdir({ rsp.parent_path() }, true));
            commands.push_back("@echo > " + rsps);
            for (auto &a : c.args)
            {
                if (should_print(a))
                    commands.push_back("@echo \"\\\"" + a + "\\\"\" >> " + rsps);
            }

            addTarget(normalize_path(rsp), {}, commands);*/
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

    static bool should_print(const String &o)
    {
        return o.find("showIncludes") == o.npos;
    };

    static String program_name(int n, bool generated = false)
    {
        String s = "SW_PROGRAM_";
        if (generated)
            s += "GENERATED_";
        return s + std::to_string(n);
    }

    String mkdir(const Files &p, bool gen = false)
    {
        if (nmake)
            return "@-if not exist " + normalize_path_windows(printFiles(p, gen)) + " mkdir " + normalize_path_windows(printFiles(p, gen));
        return "@-mkdir -p " + printFiles(p, gen);
    }
};

void MakeGenerator::generate(const Build &b)
{
    // https://www.gnu.org/software/make/manual/html_node/index.html

    const auto d = fs::absolute(path(SW_BINARY_DIR) / toPathString(type) / b.solutions[0].getConfig());

    auto ep = b.solutions[0].getExecutionPlan();

    MakeEmitter ctx;
    ctx.nmake = type == GeneratorType::NMake;
    ctx.gatherPrograms(ep.commands);

    String commands_fn = "commands.mk";
    write_file(d / commands_fn, ctx.getText());
    ctx.clear();

    ctx.include(commands_fn);
    ctx.addLine();

    // all
    Files outputs;
    for (auto &[p, t] : b.solutions[0].TargetsToBuild)
    {
        if (b.skipTarget(t->Scope))
            continue;
        if (auto nt = t->as<NativeExecutedTarget>(); nt)
        {
            auto c = nt->getCommand();
            outputs.insert(c->outputs.begin(), c->outputs.end());
        }
        else
        {
            LOG_WARN(logger, "Poor implementation of target: " << p.toString() << ". Care...");
            for (auto &c : t->getCommands())
                outputs.insert(c->outputs.begin(), c->outputs.end());
        }
    }
    ctx.addTarget("all", outputs);

    // print commands
    for (auto &c : ep.commands)
        ctx.addCommand(*c, d);

    // clean
    outputs.clear();
    for (auto &c : ep.commands)
        outputs.insert(c->outputs.begin(), c->outputs.end());
    if (ctx.nmake)
        ctx.addTarget("clean", {}, { "@del " + normalize_path_windows(MakeEmitter::printFiles(outputs, true)) });
    else
        ctx.addTarget("clean", {}, { "@rm -f " + MakeEmitter::printFiles(outputs, true) });

    write_file(d / "Makefile", ctx.getText());
}

void BatchGenerator::generate(const Build &b)
{
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
            return "SW_PROGRAM_" + std::to_string(n);
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

    const auto d = path(SW_BINARY_DIR) / toPathString(type) / b.solutions[0].getConfig();

    auto p = b.solutions[0].getExecutionPlan();

    print_commands(p, d / "commands.bat");
    print_commands_raw(p, d / "commands_raw.bat");
    print_numbers(p, d / "numbers.txt");
}

void CompilationDatabaseGenerator::generate(const Build &b)
{
    auto print_comp_db = [&b](const ExecutionPlan<builder::Command> &ep, const path &p)
    {
        if (b.solutions.empty())
            return;
        static std::set<String> exts{
            ".c", ".cpp", ".cxx", ".c++", ".cc", ".CPP", ".C++", ".CXX", ".C", ".CC"
        };
        nlohmann::json j;
        for (auto &[p, t] : b.solutions[0].children)
        {
            if (b.skipTarget(t->Scope))
                continue;
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

    const auto d = path(SW_BINARY_DIR) / toPathString(type) / b.solutions[0].getConfig();

    auto p = b.solutions[0].getExecutionPlan();

    print_comp_db(p, d / "compile_commands.json");
}

void ShellGenerator::generate(const Build &b)
{
    throw SW_RUNTIME_ERROR("not implemented");
}

}
