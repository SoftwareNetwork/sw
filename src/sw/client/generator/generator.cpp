// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "generator.h"

#include <sw/builder/file.h>
#include <sw/core/sw_context.h>

#include <primitives/sw/cl.h>
#include <primitives/win32helpers.h>

#include <boost/algorithm/string.hpp>
#include <nlohmann/json.hpp>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "generator");

namespace sw
{

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
    ProgramShortCutter1 sc;
    ProgramShortCutter1 sc_generated;

    ProgramShortCutter()
        : sc_generated("SW_PROGRAM_GENERATED_")
    {}

    String getProgramName(const String &in, const builder::Command &c)
    {
        bool gen = File(c.getProgram(), *c.fs).isGeneratedAtAll();
        auto &progs = gen ? sc_generated : sc;
        return progs.getProgramName(in);
    }

    void printPrograms(primitives::Emitter &ctx) const
    {
        auto print_progs = [&ctx](auto &a)
        {
            for (auto &kv : a)
                ctx.addLine(kv->second + " = " + kv->first);
        };

        // print programs
        print_progs(sc);
        ctx.emptyLines();
        print_progs(sc_generated);
        ctx.emptyLines();
    }
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
        g = std::make_unique<BatchGenerator>();
        break;
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
    NinjaEmitter(const SwContext &swctx, const path &dir)
    {
        const String commands_fn = "commands.ninja";
        addLine("include " + commands_fn);
        emptyLines(1);

        auto ep = swctx.getExecutionPlan();

        for (auto &c : ep.commands)
            addCommand(swctx, *c);

        primitives::Emitter ctx_progs;
        sc.printPrograms(ctx_progs);
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
            return p.u8string();
        return to_string(buf);
#else
        return p.u8string();
#endif
    }

    String prepareString(const SwContext &b, const String &s, bool quotes = false)
    {
        if (b.getHostOs().Type != OSType::Windows)
            quotes = false;

        auto s2 = s;
        boost::replace_all(s2, ":", "$:");
        boost::replace_all(s2, "\"", "\\\"");
        if (quotes)
            return "\"" + s2 + "\"";
        return s2;
    }

    void addCommand(const SwContext &b, const builder::Command &c)
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
        if (b.getHostOs().Type == OSType::Windows)
        {
            addText("cmd /S /C ");
            addText("\"");
        }
        //else
        //addText("bash -c ");

        // env
        for (auto &[k, v] : c.environment)
        {
            if (b.getHostOs().Type == OSType::Windows)
                addText("set ");
            addText(k + "=" + v + " ");
            if (b.getHostOs().Type == OSType::Windows)
                addText("&& ");
        }

        // wdir
        if (!c.working_directory.empty())
        {
            addText("cd ");
            if (b.getHostOs().Type == OSType::Windows)
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
        if (b.getHostOs().Type == OSType::Windows)
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
        for (auto &o : c.intermediate)
            addText(prepareString(b, getShortName(o)) + " ");
        addText(": c" + std::to_string(c.getHash()) + " ");
        for (auto &i : c.inputs)
            addText(prepareString(b, getShortName(i)) + " ");
        addLine();
    }
};

void NinjaGenerator::generate(const SwContext &swctx)
{
    // https://ninja-build.org/manual.html#_writing_your_own_ninja_files

    const auto dir = path(SW_BINARY_DIR) / toPathString(type) / swctx.getBuildHash();

    NinjaEmitter ctx(swctx, dir);
    write_file(dir / "build.ninja", ctx.getText());
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
            if (File(i, *c.fs).isGeneratedAtAll())
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

    static bool should_print(const String &o)
    {
        return o.find("showIncludes") == o.npos;
    }

    String mkdir(const Files &p, bool gen = false)
    {
        if (nmake)
            return "@-if not exist " + normalize_path_windows(printFiles(p, gen)) + " mkdir " + normalize_path_windows(printFiles(p, gen));
        return "@-mkdir -p " + printFiles(p, gen);
    }
};

void MakeGenerator::generate(const SwContext &b)
{
    // https://www.gnu.org/software/make/manual/html_node/index.html
    // https://en.wikipedia.org/wiki/Make_(software)

    const auto d = fs::absolute(path(SW_BINARY_DIR) / toPathString(type) / b.getBuildHash());

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
    ctx.sc.printPrograms(ctx);
    write_file(d / commands_fn, ctx.getText());
}

void BatchGenerator::generate(const SwContext &b)
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
                for (auto &a : c->arguments)
                {
                    if (should_print(a->toString()))
                        s += a->quote() + " ";
                }
                s.resize(s.size() - 1);
            }
            else
            {
                s += "@echo. 2> response.rsp\n";
                for (auto &a : c->arguments)
                {
                    if (should_print(a->toString()))
                        s += "@echo " + a->quote() + " >> response.rsp\n";
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
            s += c->getProgram() + " ";
            for (auto &a : c->arguments)
                s += a->toString() + " ";
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
            print_string(c->getProgram());
            print_string(c->working_directory.u8string());
            for (auto &a : c->arguments)
                print_string(a->toString());
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

    const auto d = path(SW_BINARY_DIR) / toPathString(type) / b.getBuildHash();

    auto p = b.getExecutionPlan();

    print_commands(p, d / "commands.bat");
    print_commands_raw(p, d / "commands_raw.bat");
    print_numbers(p, d / "numbers.txt");
}

void CompilationDatabaseGenerator::generate(const SwContext &b)
{
    static const std::set<String> exts{
        ".c", ".cpp", ".cxx", ".c++", ".cc", ".CPP", ".C++", ".CXX", ".C", ".CC"
    };

    const auto d = path(SW_BINARY_DIR) / toPathString(type) / b.getBuildHash();

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

void ShellGenerator::generate(const SwContext &b)
{
    throw SW_RUNTIME_ERROR("not implemented");
}

}
