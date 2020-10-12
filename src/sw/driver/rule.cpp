// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#include "rule.h"

#include "build.h"
#include "compiler/compiler.h"
#include "target/native.h"

#include <primitives/exceptions.h>

namespace sw
{

IRule::~IRule() = default;

NativeRule::NativeRule(RuleProgram p)
    : program(p)
{
}

NativeCompilerRule::NativeCompilerRule(RuleProgram p, const StringSet &exts)
    : NativeRule(p), exts(exts)
{
}

NativeCompiler &NativeCompilerRule::getCompiler() const
{
    return static_cast<NativeCompiler &>(program);
}

Commands NativeRule::getCommands() const
{
    Commands cmds;
    for (auto &c : commands)
        cmds.insert(c->getCommand());
    return cmds;
}

static path getObjectFilename(const Target &t, const path &p)
{
    // target may push its files to outer packages,
    // so files must be concatenated with its target name
    // ^^^ wrong?
    // target push files, they'll use local definitions etc.
    return to_string(p.filename().u8string()) + "." + sha256(
        //t.pkg.toString() +
        to_string(p.u8string())).substr(0, 8);
}

static path getOutputFile(const Target &t, const path &input)
{
    auto o = t.BinaryDir.parent_path() / "obj" / getObjectFilename(t, input);
    o = fs::absolute(o);
    return o;
}

template <class C>
static path getOutputFile(const Target &t, const C &c, const path &input)
{
    return getOutputFile(t, input) += c.getObjectExtension(t.getBuildSettings().TargetOS);
}

Files NativeCompilerRule::addInputs(const Target &t, const RuleFiles &rfs)
{
    auto nt = t.as<NativeCompiledTarget *>();
    std::optional<path> pch_basename;
    Files outputs;

    // find pch
    for (auto &rf : rfs)
    {
        if (!exts.contains(rf.getFile().extension().string()))
            continue;
        if (used_files.contains(rf))
            continue;
        // pch are only c++ feature
        if (rf.getFile().filename() == "sw.pch.cpp")
            pch_basename = normalize_path(rf.getFile().parent_path() / rf.getFile().stem());
    }

    // main loop
    for (auto &rf : rfs)
    {
        if (!exts.contains(rf.getFile().extension().string()))
            continue;
        if (used_files.contains(rf))
            continue;
        auto output = getOutputFile(t, getCompiler(), rf.getFile());
        outputs.insert(output);

        auto c = getCompiler().clone();
        auto &nc = static_cast<NativeCompiler &>(*c);
        nc.setSourceFile(rf.getFile(), output);

        // pch
        if (rf.getFile().filename() == "sw.pch.cpp")
        {
            auto setup_vs = [&pch_basename](auto C)
            {
                C->CreatePrecompiledHeader = path(*pch_basename) += ".h";
                C->PrecompiledHeaderFilename = path(*pch_basename) += ".pch";
                C->PrecompiledHeaderFilename.output_dependency = true;
            };
            auto setup_gnu = [&pch_basename, &nc, &outputs, &output, nt](auto C, auto ext)
            {
                C->Language = "c++-header";

                // set new input and output
                nc.setSourceFile(path(*pch_basename) += ".h", path(*pch_basename) += ".h"s += ext);

                // skip .obj output
                outputs.erase(output);

                // we also remove here our same input file
                if (nt)
                {
                    auto fi = nt->getMergeObject().ForceIncludes;
                    if (!fi.empty())
                    {
                        fi.erase(fi.begin());
                        C->ForcedIncludeFiles = fi;
                    }
                }
            };

            if (auto C = c->as<VisualStudioCompiler *>())
                setup_vs(C);
            else if (auto C = c->as<ClangClCompiler *>())
                setup_vs(C);
            else if (auto C = c->as<ClangCompiler *>())
                setup_gnu(C, ".pch");
            else if (auto C = c->as<GNUCompiler *>())
                setup_gnu(C, ".gch");
            else
                SW_UNIMPLEMENTED;
        }
        else if (pch_basename)
        {
            auto setup_vs = [&pch_basename](auto C)
            {
                C->UsePrecompiledHeader = path(*pch_basename) += ".h";
                C->PrecompiledHeaderFilename = path(*pch_basename) += ".pch";
                C->PrecompiledHeaderFilename.input_dependency = true;
            };
            auto setup_gnu = [&pch_basename, &t](auto C, auto ext)
            {
                // we must add this explicitly
                C->createCommand(t.getMainBuild())->addInput(path(*pch_basename) += ".h"s += ext);
            };

            if (auto C = c->as<VisualStudioCompiler *>())
                setup_vs(C);
            else if (auto C = c->as<ClangClCompiler *>())
                setup_vs(C);
            else if (auto C = c->as<ClangCompiler *>())
                setup_gnu(C, ".pch");
            else if (auto C = c->as<GNUCompiler *>())
                setup_gnu(C, ".gch");
            else
                SW_UNIMPLEMENTED;
        }

        //
        if (nt->PreprocessStep)
        {
            // pp   command: .c  -> .pp
            // base command: .pp -> .obj
            auto vs_setup = [&rf](NativeCompiler &base_command, auto &pp_command)
            {
                auto c = rf.getFile().extension() == ".c";

                pp_command.PreprocessToFile = true;
                pp_command.PreprocessFileName = base_command.getOutputFile() += (c ? ".i" : ".ii");
                pp_command.Output.clear();
                base_command.setSourceFile(pp_command.PreprocessFileName(), base_command.getOutputFile());
            };
            auto gnu_setup = [](NativeCompiler &base_command, auto &pp_command)
            {
                SW_UNIMPLEMENTED;
                // set pp
                pp_command.CompileWithoutLinking = false;
                pp_command.Preprocess = true;
                auto o = pp_command.getOutputFile();
                o = o.parent_path() / o.stem() += ".i";
                pp_command.setOutputFile(o);

                // set input file for old command
                base_command.setSourceFile(pp_command.getOutputFile(), base_command.getOutputFile());
            };

            //
            auto pp_command = c->clone();
            if (auto C = pp_command->as<VisualStudioCompiler *>())
                vs_setup(nc, *C);
            else if (auto C = pp_command->as<ClangClCompiler *>())
                vs_setup(nc, *C);
            else if (auto C = pp_command->as<ClangCompiler *>())
                gnu_setup(nc, *C);
            else if (auto C = pp_command->as<GNUCompiler *>())
                gnu_setup(nc, *C);
            else
                SW_UNIMPLEMENTED;
            static_cast<NativeCompiler &>(*pp_command).prepareCommand(t);
            commands.emplace_back(std::move(pp_command));
        }

        nc.prepareCommand(t);
        commands.emplace_back(std::move(c));
        used_files.insert(rf);
    }
    return outputs;
}

NativeLinker &NativeLinkerRule::getLinker() const
{
    return static_cast<NativeLinker &>(program);
}

Files NativeLinkerRule::addInputs(const Target &t, const RuleFiles &rfs)
{
    std::optional<path> def;
    FilesOrdered files;
    for (auto &rf : rfs)
    {
        // do not import our output
        if (rf.getFile() == getLinker().getOutputFile())
            continue;

        if (1
            && rf.getFile().extension() != ".obj"
            && rf.getFile().extension() != ".lib"
            && rf.getFile().extension() != ".res"
            && rf.getFile().extension() != ".def"
            )
            continue;

        if (rf.getFile().extension() == ".def")
        {
            if (def)
                throw SW_RUNTIME_ERROR("Muiltiple .def files are not allowed");
            def = rf.getFile();
            continue;
        }

        //if (used_files.contains(rf))
            //continue;
        files.push_back(rf.getFile());
        //used_files.insert(rf);
    }
    std::sort(files.begin(), files.end());
    if (files.empty() && !def)
        return {};

    // objs must go into object files
    // libs into library deps
    //getSelectedTool()->setObjectFiles(files);
    //getSelectedTool()->setInputLibraryDependencies(gatherLinkLibraries());

    Files outputs;

    auto c = getLinker().clone();
    static_cast<NativeLinker &>(*c).setObjectFiles(files);
    if (auto VSL = c->as<VisualStudioLibraryTool*>(); VSL && def)
        VSL->ModuleDefinitionFile = *def;
    static_cast<NativeLinker &>(*c).prepareCommand(t);
    outputs.insert(static_cast<NativeLinker &>(*c).getOutputFile());
    c->getCommand()->prepare(); // why?
    commands.clear();
    commands.emplace_back(std::move(c));

    return outputs;
}

Files RcRule::addInputs(const Target &t, const RuleFiles &rfs)
{
    Files outputs;
    for (auto &rf : rfs)
    {
        if (rf.getFile().extension() != ".rc")
            continue;
        if (used_files.contains(rf))
            continue;
        auto output = getOutputFile(t, rf.getFile()) += ".res";
        outputs.insert(output);
        auto c = program.clone();
        static_cast<RcTool &>(*c).setSourceFile(rf.getFile());
        static_cast<RcTool &>(*c).setOutputFile(output);
        static_cast<RcTool &>(*c).prepareCommand(t);
        commands.emplace_back(std::move(c));
        used_files.insert(rf);
    }
    return outputs;
}

}
