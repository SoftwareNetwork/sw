// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>

#include "generator.h"

#include <sw/builder/file.h>
#include <sw/builder/execution_plan.h>
#include <sw/core/build.h>
#include <sw/core/input.h>
#include <sw/core/sw_context.h>
#include <sw/manager/storage.h>
#include <sw/support/filesystem.h>

#include <boost/algorithm/string.hpp>
#include <nlohmann/json.hpp>
#include <primitives/sw/cl.h>
#include <primitives/pack.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "generator");

#include <cl.llvm.h>

using namespace sw;

int vsVersionFromString(const String &s);

String toPathString(VsGeneratorType t)
{
    switch (t)
    {
    case VsGeneratorType::VisualStudio:
        return "vs";
    case VsGeneratorType::VisualStudioNMake:
        return "vs_nmake";
    /*case VsGeneratorType::VisualStudioUtility:
        return "vs_util";
    case VsGeneratorType::VisualStudioNMakeAndUtility:
        return "vs_nmake_util";*/
    default:
        throw SW_LOGIC_ERROR("not implemented");
    }
}

static std::vector<GeneratorDescription> createGenerators()
{
    std::vector<GeneratorDescription> g;

#define GENERATOR(x, y)                                           \
    {                                                             \
        GeneratorDescription d;                                   \
        d.type = GeneratorType::x;                                \
        d.name = y;                                               \
        d.path_string = boost::to_lower_copy(String(y));          \
        d.allowed_names.insert(boost::to_lower_copy(String(#x))); \
        d.allowed_names.insert(boost::to_lower_copy(String(y)));  \
        g.push_back(d);                                           \
    }
#include "generator.inl"
#undef GENERATOR

    // correct path strings

#define SET_PATH_STRING(t, n) \
    g[(int)t].path_string = boost::to_lower_copy(String(n))

    SET_PATH_STRING(GeneratorType::FastBuild, "fbuild");
    SET_PATH_STRING(GeneratorType::CompilationDatabase, "compdb");
    SET_PATH_STRING(GeneratorType::RawBootstrapBuild, "rawbootstrap");
    SET_PATH_STRING(GeneratorType::SwExecutionPlan, "swexplan");
    SET_PATH_STRING(GeneratorType::SwBuildDescription, "swbdesc");
    SET_PATH_STRING(GeneratorType::CodeBlocks, "cb");
    SET_PATH_STRING(GeneratorType::VisualStudio, "vs");

#undef SET_PATH_STRING

    // additional allowed names

#define ADD_ALLOWED_NAME(t, n) \
    g[(int)t].allowed_names.insert(boost::to_lower_copy(String(n)))

    ADD_ALLOWED_NAME(GeneratorType::VisualStudio, "VS");
    ADD_ALLOWED_NAME(GeneratorType::VisualStudio, "VS_IDE");
    ADD_ALLOWED_NAME(GeneratorType::VisualStudio, "VS_NMake");
    ADD_ALLOWED_NAME(GeneratorType::VisualStudio, "VSNMake");
    ADD_ALLOWED_NAME(GeneratorType::CodeBlocks, "cb");
    ADD_ALLOWED_NAME(GeneratorType::Make, "Makefile");
    ADD_ALLOWED_NAME(GeneratorType::FastBuild, "FBuild");
    ADD_ALLOWED_NAME(GeneratorType::CompilationDatabase, "CompDb");
    ADD_ALLOWED_NAME(GeneratorType::SwExecutionPlan, "SwExPlan");
    ADD_ALLOWED_NAME(GeneratorType::SwBuildDescription, "SwBDesc");
    ADD_ALLOWED_NAME(GeneratorType::RawBootstrapBuild, "rawbootstrap");
    ADD_ALLOWED_NAME(GeneratorType::RawBootstrapBuild, "raw-bootstrap");

    //else if (boost::iequals(s, "qtc"))
    //return GeneratorType::qtc;

#undef ADD_ALLOWED_NAME

    return g;
}

const std::vector<GeneratorDescription> &getGenerators()
{
    static const auto g = createGenerators();
    return g;
}

void checkForSingleSettingsInputs(const SwBuild &b)
{
    for (auto &i : b.getInputs())
    {
        if (i.getSettings().size() != 1)
            throw SW_RUNTIME_ERROR("This generator supports single config inputs only.");
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
    /*case VsGeneratorType::VisualStudioUtility:
        return "Visual Studio Utility";
    case VsGeneratorType::VisualStudioNMakeAndUtility:
        return "Visual Studio NMake and Utility";*/
    default:
        throw SW_LOGIC_ERROR("not implemented");
    }
}

static GeneratorType fromString(const String &s)
{
    auto s2 = boost::to_lower_copy(s);
    for (auto &g : getGenerators())
    {
        if (g.allowed_names.find(s2) != g.allowed_names.end())
            return g.type;
    }

    //String gens;
    //for (int i = 0; i < (int)GeneratorType::Max; i++)
        //gens += "    - " + toString(GeneratorType(i)) + "\n";
    throw SW_RUNTIME_ERROR("Unknown generator: " + s
        //+ "\nAvailable generators:\n" + gens
    );
}

static VsGeneratorType fromStringVs(const String &s)
{
    if (0);

    else if (0
        || boost::istarts_with(s, "VS_IDE")
        || boost::iequals(s, "VS")
        || boost::iequals(s, "Visual Studio")
        )
        return VsGeneratorType::VisualStudio;

    else if (
        boost::istarts_with(s, "VS_NMake") ||
        boost::istarts_with(s, "VSNMake"))
        return VsGeneratorType::VisualStudioNMake;

    /*else if (
        boost::istarts_with(s, "VS_Utility") ||
        boost::istarts_with(s, "VS_Util") ||
        boost::istarts_with(s, "VSUtil"))
        return VsGeneratorType::VisualStudioUtility;

    else if (
        boost::istarts_with(s, "VS_NMakeAndUtility") ||
        boost::istarts_with(s, "VS_NMakeAndUtil") ||
        boost::istarts_with(s, "VS_NMakeUtil") ||
        boost::istarts_with(s, "VSNMakeAndUtil") ||
        boost::istarts_with(s, "VSNMakeUtil"))
        return VsGeneratorType::VisualStudioNMakeAndUtility;*/

    throw SW_RUNTIME_ERROR("Unknown VS generator: " + s);
}

Generator::Generator(const Options &options)
    : options(options)
{
}

std::unique_ptr<Generator> Generator::create(const Options &options)
{
#define CREATE_GENERATOR(t) std::make_unique<t>(options)

    auto t = fromString(options.options_generate.generator);
    std::unique_ptr<Generator> g;
    switch (t)
    {
    case GeneratorType::VisualStudio:
    {
        auto g1 = CREATE_GENERATOR(VSGenerator);
        g1->vstype = fromStringVs(options.options_generate.generator);
        //if (g1->vstype > VsGeneratorType::VisualStudioNMake)
            //SW_UNIMPLEMENTED;
        g = std::move(g1);
        break;
    }
    case GeneratorType::CodeBlocks:
        g = CREATE_GENERATOR(CodeBlocksGenerator);
        break;
    case GeneratorType::Xcode:
        g = CREATE_GENERATOR(XcodeGenerator);
        break;
    case GeneratorType::Ninja:
        g = CREATE_GENERATOR(NinjaGenerator);
        break;
    case GeneratorType::CMake:
        g = CREATE_GENERATOR(CMakeGenerator);
        break;
    case GeneratorType::FastBuild:
        g = CREATE_GENERATOR(FastBuildGenerator);
        break;
    case GeneratorType::NMake:
    case GeneratorType::Make:
        g = CREATE_GENERATOR(MakeGenerator);
        break;
    case GeneratorType::Batch:
    {
        auto g1 = CREATE_GENERATOR(ShellGenerator);
        g1->batch = true;
        g = std::move(g1);
        break;
    }
    case GeneratorType::Shell:
        g = CREATE_GENERATOR(ShellGenerator);
        break;
    case GeneratorType::CompilationDatabase:
        g = CREATE_GENERATOR(CompilationDatabaseGenerator);
        break;
    case GeneratorType::SwExecutionPlan:
        g = CREATE_GENERATOR(SwExecutionPlanGenerator);
        break;
    case GeneratorType::SwBuildDescription:
        g = CREATE_GENERATOR(SwBuildDescriptionGenerator);
        break;
    case GeneratorType::RawBootstrapBuild:
        g = CREATE_GENERATOR(RawBootstrapBuildGenerator);
        break;
    default:
        SW_UNIMPLEMENTED;
    }
    g->type = t;
    return g;
}

path Generator::getRootDirectory(const sw::SwBuild &b) const
{
    return b.getBuildDirectory() / "g" / getPathString() / b.getName();
}

path Generator::getPathString() const
{
    return getGenerators()[(int)getType()].path_string;
}

path VSGenerator::getPathString() const
{
    auto s = toPathString(vstype);
    if (compiler_type == ClangCl)
        s += "_clangcl";
    else if (compiler_type == Clang)
        s += "_clang";
    else if (compiler_type == MSVC)
        ;// s += "_msvc";
    else
        SW_UNIMPLEMENTED;
    return s;
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

        auto explan = b.getExecutionPlan();
        auto &ep = *explan;

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
            return to_string(normalize_path(p));
        return to_string(normalize_path(to_string(buf)));
#else
        return to_string(normalize_path(p));
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
        if (!c.msvc_prefix.empty())
            addLine("msvc_deps_prefix = \"" + c.msvc_prefix + "\"");
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
            addText("@" + to_string(rsp_file.u8string()) + " ");
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
            addLine("depfile = " + to_string((c.outputs.begin()->parent_path() / (c.outputs.begin()->stem().string() + ".d")).u8string()));
        if (rsp)
        {
            addLine("rspfile = " + to_string(rsp_file.u8string()));
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
        addKeyValue(key, "\"" + to_string(normalize_path(value)) + "\"");
    }

    void include(const path &fn)
    {
        addLine("include " + to_string(normalize_path(fn)));
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
            s += "cd \"" + to_string(normalize_path(c.working_directory)) + "\" && ";

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
            s += "@" + to_string(normalize_path(rsp));

        if (!c.in.file.empty())
            s += " < " + to_string(normalize_path(c.in.file));
        if (!c.out.file.empty())
            s += " > " + to_string(normalize_path(c.out.file));
        if (!c.err.file.empty())
            s += " 2> " + to_string(normalize_path(c.err.file));

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
        s += to_string(normalize_path(p));
        if (!quotes)
            boost::replace_all(s, " ", "\\\\ ");
        if (quotes)
            s += "\"";
        return s;
    }

    String mkdir(const Files &p, bool gen = false)
    {
        if (nmake)
        {
            return "@-if not exist " + to_string(normalize_path_windows(printFiles(p, gen))) +
                " mkdir " + to_string(normalize_path_windows(printFiles(p, gen)));
        }
        return "@-mkdir -p " + printFiles(p, gen);
    }
};

void MakeGenerator::generate(const SwBuild &b)
{
    // https://www.gnu.org/software/make/manual/html_node/index.html
    // https://en.wikipedia.org/wiki/Make_(software)

    const auto d = getRootDirectory(b);

    auto explan = b.getExecutionPlan();
    auto &ep = *explan;

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
        ctx.addTarget("clean", {}, { "@del " + to_string(normalize_path_windows(MakeEmitter::printFiles(outputs, true))) });
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
    bool is_generated_ext(const path &);

    auto inputs = b.getInputs();
    if (inputs.size() != 1)
        throw SW_RUNTIME_ERROR("Only single input is supported at the moment");
    if (inputs[0].getSettings().size() != 1)
        throw SW_RUNTIME_ERROR("Only single settings is supported at the moment");
    SW_UNIMPLEMENTED;
    bool abs_pkg = false;// inputs[0].getInput().getType() == sw::InputType::InstalledPackage;

    auto ep = b.getExecutionPlan();

    primitives::Emitter ctx;
    const auto long_line = "################################################################################";

    auto add_title = [&ctx, &long_line](const String &title)
    {
        ctx.addLine(long_line);
        ctx.addLine("#");
        ctx.addLine("# " + title);
        ctx.addLine("#");
        ctx.addLine(long_line);
        ctx.addLine();
    };

    add_title("This is SW generated file. Do not edit!");

    ctx.addLine("cmake_minimum_required(VERSION 3.12.0)");
    ctx.addLine();
    ctx.addLine("project("s + "sw" /*b.getName()*/ + " LANGUAGES C CXX)"); // ASM
    ctx.addLine();

    auto &ctx_deps = ctx.createInlineEmitter();

    StringSet deps;
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
        if (!abs_pkg && pkg.getPath().isAbsolute())
            continue;

        auto &t = **tgts.begin();
        const auto &s = t.getInterfaceSettings();

        add_title("Target: " + pkg.toString());

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
        ctx.addLine();

        /*auto can_add_file = [](const auto &f)
        {
            auto t = get_vs_file_type_by_ext(f);
            return t == VSFileType::ClInclude || t == VSFileType::None;
        };*/

        Files files;
        auto cmds = t.getCommands();
        for (auto &c : cmds)
        {
            for (auto &o : c->inputs)
            {
                if (is_generated_ext(o))
                    continue;

                //if (can_add_file(o))
                    files.insert(o);
                /*else
                    d.build_rules[c.get()] = o;*/
            }

            for (auto &o : c->outputs)
            {
                if (is_generated_ext(o))
                    continue;

                //if (can_add_file(o))
                    files.insert(o);

                /*if (1
                    && c->arguments.size() > 1
                    && c->arguments[1]->toString() == sw::builder::getInternalCallBuiltinFunctionName()
                    && c->arguments.size() > 3
                    && c->arguments[3]->toString() == "sw_create_def_file"
                    )
                {
                    d.pre_link_command = c.get();
                    continue;
                }

                d.custom_rules.insert(c.get());*/
            }
        }

        ctx.addLine("target_sources(" + pkg.toString() + " PRIVATE");
        ctx.increaseIndent();
        for (auto &f : files)
            ctx.addLine(to_string(normalize_path(f)));
        ctx.decreaseIndent();
        ctx.addLine(")");
        ctx.addLine();

        ctx.addLine("target_compile_definitions(" + pkg.toString() + " PRIVATE");
        ctx.increaseIndent();
        // TODO: fix properties like in integration
        for (auto &[k, v] : s["this"]["definitions"].getMap())
        {
            if (k == "NDEBUG")
                continue;
            ctx.addLine("\"" + k + "=" + v.getValue() + "\"");
        }
        ctx.decreaseIndent();
        ctx.addLine(")");
        ctx.addLine();

        ctx.addLine("target_include_directories(" + pkg.toString() + " PRIVATE");
        ctx.increaseIndent();
        for (auto &f : s["this"]["include_directories"].getArray())
            ctx.addLine("\"" + to_string(normalize_path(f.getPathValue(b.getContext().getLocalStorage()))) + "\"");
        ctx.decreaseIndent();
        ctx.addLine(")");
        ctx.addLine();

        ctx.addLine("target_link_libraries(" + pkg.toString() + " PRIVATE");
        ctx.increaseIndent();
        for (auto &[k, _] : s["dependencies"]["link"].getMap())
        {
            if (sw::PackageId(k).getPath().isAbsolute())
                deps.insert(k);
            ctx.addLine(k);
        }
        if (!s["dependencies"]["link"].getMap().empty())
            ctx.addLine();
        for (auto &f : s["this"]["system_link_libraries"].getArray())
            ctx.addLine("\"" + to_string(normalize_path(f.getValue())) + "\"");
        ctx.decreaseIndent();
        ctx.addLine(")");
        ctx.addLine();

        ctx.addLine("target_compile_options(" + pkg.toString() + " PRIVATE");
        ctx.increaseIndent();
        for (auto &f : s["this"]["compile_options"].getArray())
            ctx.addLine("\"" + f.getValue() + "\"");
        ctx.decreaseIndent();
        ctx.addLine(")");
        ctx.addLine();

        ctx.addLine("target_link_options(" + pkg.toString() + " PRIVATE /NODEFAULTLIB)");
        ctx.addLine("target_link_options(" + pkg.toString() + " PRIVATE");
        ctx.increaseIndent();
        for (auto &f : s["this"]["link_options"].getArray())
            ctx.addLine("\"" + f.getValue() + "\"");
        ctx.decreaseIndent();
        ctx.addLine(")");
        ctx.addLine();

        ctx.emptyLines();
    }

    if (!deps.empty())
    {
        ctx_deps.addLine("find_package(SW REQUIRED)");
        ctx_deps.addLine("sw_add_package(");
        ctx_deps.increaseIndent();
        for (auto &d : deps)
            ctx_deps.addLine(d);
        ctx_deps.decreaseIndent();
        ctx_deps.addLine(")");
        ctx_deps.addLine("sw_execute()");
        ctx_deps.addLine();
    }

    ctx.addLine(long_line);
    ctx.addLine();

    write_file(getRootDirectory(b) / "CMakeLists.txt", ctx.getText());
}

void FastBuildGenerator::generate(const sw::SwBuild &b)
{
    // https://www.fastbuild.org/docs/functions/exec.html

    auto explan = b.getExecutionPlan();
    auto &ep = *explan;

    primitives::CppEmitter ctx;
    for (auto &c1 : ep.getCommands())
    {
        auto c = static_cast<builder::Command *>(c1);
        ctx.addLine("Exec( \"" + std::to_string(c->getHash()) + "\" )");
        ctx.beginBlock();

        // wdir
        if (!c->working_directory.empty())
            ctx.addLine(".ExecWorkingDir = \"" + to_string(normalize_path(c->working_directory)) + "\"");

        // has no support for env vars?
        // env
        for (auto &[k, v] : c->environment)
            ;

        ctx.addLine(".ExecExecutable = \"" + to_string(normalize_path(c->getProgram())) + "\"");

        ctx.addLine(".ExecArguments = \"");
        bool exe_skipped = false;
        for (auto &a : c->arguments)
        {
            if (!exe_skipped)
            {
                exe_skipped = true;
                continue;
            }
            auto s = a->toString();
            auto q = s[0] == '\"' ? "^" : "^\"";
            ctx.addText(q + s + q + " ");
        }
        ctx.trimEnd(1);
        ctx.addText("\"");

        ctx.addLine(".ExecInput = \"");
        for (auto &i : c->inputs)
            ctx.addText(to_string(normalize_path(i)) + " ");
        ctx.trimEnd(1);
        ctx.addText("\"");

        ctx.addLine(".ExecOutput = \"");
        for (auto &i : c->outputs)
            ctx.addText(to_string(normalize_path(i)) + " ");
        ctx.trimEnd(1);
        ctx.addText("\"");

        ctx.endBlock();
        ctx.emptyLines(1);
    }

    write_file(getRootDirectory(b) / "fbuild.bff", ctx.getText());
}

void ShellGenerator::generate(const SwBuild &b)
{
    auto explan = b.getExecutionPlan();
    auto &ep = *explan;

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
            ctx.addText("cd \"" + to_string(normalize_path(c->working_directory)) + "\" && ");

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
                ctx.addText(" < " + to_string(normalize_path(c->in.file)));
            if (!c->out.file.empty())
                ctx.addText(" > " + to_string(normalize_path(c->out.file)));
            if (!c->err.file.empty())
                ctx.addText(" 2> " + to_string(normalize_path(c->err.file)));
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
        ctx.addLine(alias + "=\"" + to_string(normalize_path(prog)) + "\"");
    });

    write_file(getRootDirectory(b) / ("commands"s + (batch ? ".bat" : ".sh")), ctx.getText());
}

void CompilationDatabaseGenerator::generate(const SwBuild &b)
{
    static const std::set<String> exts{
        ".c", ".cpp", ".cxx", ".c++", ".cc", ".CPP", ".C++", ".CXX", ".C", ".CC"
    };

    checkForSingleSettingsInputs(b);

    const auto d = getRootDirectory(b);

    auto p = b.getExecutionPlan();

    nlohmann::json j;
    for (auto &[p, tgts] : b.getTargetsToBuild())
    {
        for (auto &tgt : tgts)
        {
            for (auto &c : tgt->getCommands())
            {
                nlohmann::json j2;
                if (!c->working_directory.empty())
                    j2["directory"] = to_printable_string(normalize_path(c->working_directory));
                if (!c->inputs.empty())
                {
                    bool cppset = false;
                    for (auto &input : c->inputs)
                    {
                        auto i = exts.find(input.extension().string());
                        if (i == exts.end())
                            continue;
                        j2["file"] = to_printable_string(normalize_path(input));
                        cppset = true;
                        break;
                    }
                    if (!cppset)
                    {
                        for (auto &input : c->inputs)
                        {
                            if (normalize_path(input) != normalize_path(c->getProgram()))
                            {
                                j2["file"] = to_printable_string(normalize_path(input));
                                break;
                            }
                        }
                    }
                }
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
    ep->save(fn);
}

void SwBuildDescriptionGenerator::generate(const sw::SwBuild &b)
{
    const auto d = getRootDirectory(b);
    auto fn = path(d) += ".json";
    fs::create_directories(d.parent_path());

    nlohmann::json jx;
    jx["schema"]["version"] = 1;
    auto &j = jx["build"];
    for (auto &[pkg, tgts] : b.getTargets())
    //for (auto &[pkg, tgts] : b.getTargetsToBuild())
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
            j[boost::to_lower_copy(pkg.toString())].push_back(j1);
        }
    }
    write_file(fn, jx.dump(4));
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
    for (auto &c1 : ep->getCommands())
    {
        auto &c = dynamic_cast<const sw::builder::Command &>(*c1);
        files.insert(c.inputs.begin(), c.inputs.end());
        files.insert(c.implicit_inputs.begin(), c.implicit_inputs.end());
    }

    LOG_INFO(logger, "Filtering files");

    const auto cp = fs::current_path();
    const auto sd = b.getContext().getLocalStorage().storage_dir;

    // filter out files not in current dir and not in storage
    std::map<path /* real file */, path /* path in archive */> files2;
    FilesSorted files_ordered;
    for (auto &f : files)
    {
        if (File(f, b.getFileStorage()).isGenerated())
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
        s += to_string(normalize_path(f)) + "\n";
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
    script += "cd \"" + to_string(normalize_path(fs::current_path())) + "\"\n";
    script += "ninja -C \"" + to_string(normalize_path(dir)) + "\"\n";
    write_file(script_fn, script);

    pack_files("bootstrap.tar.xz", files2);
}

void CodeBlocksGenerator::generate(const sw::SwBuild &b)
{
    // http://wiki.codeblocks.org/index.php/Project_file
    SW_UNIMPLEMENTED;
}

void XcodeGenerator::generate(const sw::SwBuild &b)
{
    // http://www.monobjc.net/xcode-project-file-format.html
    SW_UNIMPLEMENTED;
}
