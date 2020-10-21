// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#include "rule.h"

#include "build.h"
#include "command.h"
#include "extensions.h"
#include "compiler/compiler.h"
#include "target/native.h"

#include <sw/builder/jumppad.h>

#include <primitives/exceptions.h>

void createDefFile(const path &def, const Files &obj_files)
#if defined(CPPAN_OS_WINDOWS)
;
#else
{}
#endif

static int create_def_file(path def, Files obj_files)
{
    createDefFile(def, obj_files);
    return 0;
}
SW_DEFINE_VISIBLE_FUNCTION_JUMPPAD(sw_create_def_file, create_def_file)

namespace sw
{

struct TargetFilenames
{
    String sd, bd, bdp;

    TargetFilenames(const Target &t)
    {
        sd = to_string(normalize_path(t.SourceDir));
        bd = to_string(normalize_path(t.BinaryDir));
        bdp = to_string(normalize_path(t.BinaryPrivateDir));
    }

    String getName(const path &fn) const
    {
        auto p = to_string(normalize_path(fn));
        if (bdp.size() < p.size() && p.find(bdp) == 0)
        {
            auto n = p.substr(bdp.size());
            return "[bdir_pvt]" + n;
        }
        else if (bd.size() < p.size() && p.find(bd) == 0)
        {
            auto n = p.substr(bd.size());
            return "[bdir]" + n;
        }
        if (sd.size() < p.size() && p.find(sd) == 0)
        {
            auto n = p.substr(sd.size());
            return n;
        }
        return p;
    }
};

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
    for (auto &c : commands2)
        cmds.insert(c);
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

Files NativeCompilerRule::addInputs(Target &t, const RuleFiles &rfs)
{
    auto nt = t.as<NativeCompiledTarget *>();
    std::optional<path> pch_basename;
    RuleFiles rfs_new;
    TargetFilenames tfns(t);

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

    // unity build
    if (nt && nt->UnityBuild)
    {
        /*std::vector<NativeSourceFile *> files2(files.begin(), files.end());
        std::sort(files2.begin(), files2.end(), [](const auto f1, const auto f2)
        {
            return f1->index < f2->index;
        });*/

        struct data
        {
            String s;
            int idx = 0;
            String ext;
        };

        data c, cpp;
        c.ext = ".c";
        cpp.ext = ".cpp";
        int fidx = 1; // for humans
        auto writef = [nt, &fidx, &rfs_new](auto &d)
        {
            if (d.s.empty())
                return;
            auto fns = "Module." + std::to_string(fidx++) + d.ext;
            auto fn = nt->BinaryPrivateDir / "unity" / fns;
            write_file_if_different(fn, d.s); // do not trigger rebuilds
            //getMergeObject()[fn].fancy_name = "[" + getPackage().toString() + "]/[unity]/" + fns;
            d.s.clear();
            rfs_new.insert(fn);
        };

        for (auto &rf : rfs)
        {
            // skip when args are populated
            if (!rf.getAdditionalArguments().empty())
            {
                rfs_new.insert(rf);
                continue;
            }

            auto ext = rf.getFile().extension().string();
            auto cext = ext == ".c";
            // TODO: .m .mm files?
            auto cppext = getCppSourceFileExtensions().find(ext) != getCppSourceFileExtensions().end();
            // skip asm etc.
            if (!cext && !cppext)
                continue;

            // asm won't work here right now
            data &d = cext ? c : cpp;
            d.s += "#include \"" + to_string(normalize_path(rf.getFile())) + "\"\n";
            if (++d.idx % nt->UnityBuildBatchSize == 0)
                writef(d);
        }
        writef(c);
        writef(cpp);
    }

    // main loop
    Files outputs;
    for (auto &rf : rfs_new.empty() ? rfs : rfs_new)
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
        /*nc.getCommand()->name = rulename;
        if (!rulename.empty())
            nc.getCommand()->name += " ";*/
        nc.getCommand()->name += "[" + t.getPackage().toString() + "]" + tfns.getName(rf.getFile());
        commands.emplace_back(std::move(c));
        used_files.insert(rf);
    }
    return outputs;
}

NativeLinker &NativeLinkerRule::getLinker() const
{
    return static_cast<NativeLinker &>(program);
}

Files NativeLinkerRule::addInputs(Target &t, const RuleFiles &rfs)
{
    auto nt = t.as<NativeCompiledTarget *>();
    // librarian otherwise
    auto is_linker = &getLinker() == &nt->getLinker();

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
            && rf.getFile().extension() != ".exp"
            )
            continue;

        if (rf.getFile().extension() == ".def")
        {
            if (def)
                throw SW_RUNTIME_ERROR("Muiltiple .def files are not allowed");
            def = rf.getFile();
            continue;
        }

        // lib skips these
        if (!is_linker &&
            (rf.getFile().extension() == ".res" ||
            rf.getFile().extension() == ".exp")
            )
            continue;

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
    auto &nc = static_cast<NativeLinker &>(*c);
    if (auto VSL = c->as<VisualStudioLibraryTool *>())
    {
        // export all symbols
        if (1
            // ignore if def is already present
            && !def
            // check setting
            && nt && nt->ExportAllSymbols
            // win only
            && nt->getBuildSettings().TargetOS.Type == OSType::Windows
            // linker only
            && is_linker
            )
        {
            const path deffn = nt->BinaryPrivateDir / ".sw.symbols.def";
            Files objs;
            for (auto &f : files)
            {
                if (f.extension() == ".obj")
                    objs.insert(f);
            }
            auto c = nt->addCommand(SW_VISIBLE_BUILTIN_FUNCTION(create_def_file));
            c->push_back(deffn);
            c->addOutput(deffn);
            //c << cmd::out(deffn);
            std::dynamic_pointer_cast<builder::BuiltinCommand>(c.getCommand())->push_back(objs);
            c->addInput(objs);
            def = deffn;
            commands2.insert(c.getCommand());
        }
        if (def)
            VSL->ModuleDefinitionFile = *def;
        if (nt && nt->hasCircularDependency())
        {
            if (auto VSL = c->as<VisualStudioLibrarian *>())
            {
                // add -DEF
                VSL->CreateImportLibrary = true;
                // set proper name (default is not suitable for .exe case)
                VSL->DllName = nt->getOutputFile().filename();

                // add llibs and ldirs
                for (auto &l : VSL->gatherLinkLibraries(true))
                    files.push_back(l.l);
                VSL->VisualStudioLibraryToolOptions::LinkDirectories = VSL->gatherLinkDirectories();
            }
            if (is_linker)
            {
                auto exp = nt->getImportLibrary();
                exp = exp.parent_path() / (exp.stem() += ".exp");
                files.erase(std::remove(files.begin(), files.end(), nt->getImportLibrary()), files.end());
                files.push_back(exp);
                VSL->ImportLibrary.clear(); // clear implib
            }
        }
    }
    nc.setObjectFiles(files);
    nc.prepareCommand(t);
    if (nt && nt->hasCircularDependency())
    {
        if (auto VSL = c->as<VisualStudioLibrarian *>())
        {
            auto exp = nt->getImportLibrary();
            exp = exp.parent_path() / (exp.stem() += ".exp");
            c->getCommand()->addOutput(exp);
            //outputs.insert(exp); // we can live without it
        }
    }
    outputs.insert(nc.getOutputFile());
    c->getCommand()->prepare(); // why?
    c->getCommand()->name = //(is_linker ? "[LINK]"s : "[LIB]"s) + " " +
        "[" + t.getPackage().toString() + "]" + nt->getOutputFile().extension().string();
    nt->registerCommand(*c->getCommand());
    commands.clear();
    commands.emplace_back(std::move(c));

    return outputs;
}

Files RcRule::addInputs(Target &t, const RuleFiles &rfs)
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
