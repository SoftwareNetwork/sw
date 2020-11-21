// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#include "rule.h"

#include "build.h"
#include "command.h"
#include "extensions.h"
#include "compiler/compiler.h"
#include "compiler/rc.h"
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

IRule::~IRule() = default;

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

NativeRule::NativeRule(RuleProgram p)
    : program(std::move(p))
{
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

path NativeRule::getOutputFileBase(const Target &t, const path &input)
{
    auto o = t.BinaryDir.parent_path() / "obj" / getObjectFilename(t, input);
    o = fs::absolute(o);
    return o;
}

path NativeRule::getOutputFile(const Target &t, const path &input)
{
    return getOutputFileBase(t, input) += t.getBuildSettings().TargetOS.getObjectFileExtension();
}

void NativeRule::addInputs(const Target &t, RuleFiles &rfs)
{
    RuleFiles rfs_new;
    for (auto &[_, rf] : rfs)
    {
        //if (!rf.isNew()) continue;
        auto rfs2 = addInput(t, rfs, rf);
        rfs_new.merge(rfs2);
    }
    rfs.merge(rfs_new);
}

void NativeCompilerRule::setup(const Target &t)
{
    auto &prog = static_cast<NativeCompiler &>(*program);
    auto nt = t.as<NativeCompiledTarget *>();
    if (!nt)
        return;

    switch (lang)
    {
    case LANG_ASM:
        exts = get_asm_exts(nt->getBuildSettings().TargetOS.is(OSType::Windows));
        if (auto c = prog.as<VisualStudioASMCompiler *>(); c && nt->getBuildSettings().TargetOS.Arch == ArchType::x86)
            c->SafeSEH = true;
        break;
    case LANG_C:
        exts.insert(".c");
        break;
    case LANG_CPP:
        exts = get_cpp_exts(nt->getBuildSettings().TargetOS.isApple());
        break;
    }

    // setup
    auto vs_setup = [this, nt, &prog](auto *c)
    {
        if (nt->getBuildSettings().Native.MT)
            c->RuntimeLibrary = vs::RuntimeLibraryType::MultiThreaded;

        switch (nt->getBuildSettings().Native.ConfigurationType)
        {
        case ConfigurationType::Debug:
            c->RuntimeLibrary =
                nt->getBuildSettings().Native.MT ?
                vs::RuntimeLibraryType::MultiThreadedDebug :
                vs::RuntimeLibraryType::MultiThreadedDLLDebug;
            c->Optimizations().Disable = true;
            break;
        case ConfigurationType::Release:
            c->Optimizations().FastCode = true;
            break;
        case ConfigurationType::ReleaseWithDebugInformation:
            c->Optimizations().FastCode = true;
            break;
        case ConfigurationType::MinimalSizeRelease:
            c->Optimizations().SmallCode = true;
            break;
        }
        if (!isC())
            c->CPPStandard = nt->CPPVersion;
        // else
        // TODO: ms now has C standard since VS16.8?
    };

    auto gnu_setup = [this, nt, &prog](auto *c)
    {
        switch (nt->getBuildSettings().Native.ConfigurationType)
        {
        case ConfigurationType::Debug:
            c->GenerateDebugInformation = true;
            //c->Optimizations().Level = 0; this is the default
            break;
        case ConfigurationType::Release:
            c->Optimizations().Level = 3;
            break;
        case ConfigurationType::ReleaseWithDebugInformation:
            c->GenerateDebugInformation = true;
            c->Optimizations().Level = 2;
            break;
        case ConfigurationType::MinimalSizeRelease:
            c->Optimizations().SmallCode = true;
            c->Optimizations().Level = 2;
            break;
        }
        if (!isC())
            c->CPPStandard = nt->CPPVersion;
        else
            c->CStandard = nt->CVersion;

        if (nt->ExportAllSymbols && nt->getRealType() != TargetType::NativeStaticLibrary)
            c->VisibilityHidden = false;
    };

    if (auto c = prog.as<VisualStudioCompiler*>())
    {
        /*if (UseModules)
        {
        c->UseModules = UseModules;
        //c->stdIfcDir = c->System.IncludeDirectories.begin()->parent_path() / "ifc" / (getBuildSettings().TargetOS.Arch == ArchType::x86_64 ? "x64" : "x86");
        c->stdIfcDir = c->System.IncludeDirectories.begin()->parent_path() / "ifc" / c->file.parent_path().filename();
        c->UTF8 = false; // utf8 is not used in std modules and produce a warning

        auto s = read_file(f->file);
        std::smatch m;
        static std::regex r("export module (\\w+)");
        if (std::regex_search(s, m, r))
        {
        c->ExportModule = true;
        }
        }*/

        vs_setup(c);
    }
    else if (auto c = prog.as<ClangClCompiler*>())
    {
        vs_setup(c);

        // we do everything ourselves
        // otherwise we get wrong order on clang includes and msvc includes (intrinsics and such)
        c->getCommand()->push_back("-nostdinc");

        // clang gives error on reinterpret cast in offsetof macro in win ucrt
        c->add(Definition("_CRT_USE_BUILTIN_OFFSETOF"));

        switch (nt->getBuildSettings().TargetOS.Arch)
        {
        case ArchType::x86_64:
            c->CommandLineOptions<ClangClOptions>::Arch = clang::ArchType::m64;
            break;
        case ArchType::x86:
            c->CommandLineOptions<ClangClOptions>::Arch = clang::ArchType::m32;
            break;
        case ArchType::arm:
        {
            c->getCommand()->push_back("--target=arm-pc-windows-msvc");
            // set using target? check correctness then: improve getTargetTriplet()
        }
        break;
        case ArchType::aarch64:
        {
            c->getCommand()->push_back("--target=aarch64-pc-windows-msvc");
            // set using target? check correctness then: improve getTargetTriplet()
        }
        break;
        default:
            throw SW_RUNTIME_ERROR("Unknown arch");
        }
    }
    // clang compiler is not working atm, gnu is created instead
    else if (auto c = prog.as<ClangCompiler*>())
    {
        gnu_setup(c);

        c->Target = nt->getBuildSettings().getTargetTriplet();
        if (nt->getBuildSettings().TargetOS.is(OSType::Windows))
        {
            // this one leaves default clang runtime library include path (from installed dir)
            c->getCommand()->push_back("-nostdlibinc");
            // this one cleans all default include dirs
            //c->push_back("-nostdinc");
            // clang gives error on reinterpret cast in offsetof macro in win ucrt
            c->add(Definition("_CRT_USE_BUILTIN_OFFSETOF"));
        }
        if (nt->getBuildSettings().TargetOS.isApple())
        {
            if (nt->getBuildSettings().TargetOS.Version)
            {
                c->getCommand()->push_back("-mmacosx-version-min=" + nt->getBuildSettings().TargetOS.Version->toString());
            }
            //C->VisibilityHidden = false;
            //C->VisibilityInlinesHidden = false;
            //C->PositionIndependentCode = false;
        }
    }
    else if (auto c = prog.as<GNUCompiler*>())
    {
        gnu_setup(c);
    }

    // merge settings
    if (!nt->isHeaderOnly())
    {
        prog.merge(*nt);
    }
}

void NativeCompilerRule::addInputs(const Target &t, RuleFiles &rfs)
{
    auto &cl = static_cast<NativeCompiler &>(*program);
    auto nt = t.as<NativeCompiledTarget *>();
    std::optional<path> provided_pdb;
    std::optional<path> provided_pch;
    std::optional<path> provided_pchh;
    RuleFiles rfs_unity;
    TargetFilenames tfns(t);

    // find pch/pdb
    for (auto &[_,rf] : rfs)
    {
        if (rf.getFile().extension() == ".pdb")
        {
            if (provided_pdb)
                throw SW_RUNTIME_ERROR("More than one pdb passed");
            provided_pdb = rf.getFile();
        }
        if (rf.getFile().extension() == ".pch")
        {
            if (provided_pch)
                throw SW_RUNTIME_ERROR("More than one pch passed");
            provided_pch = rf.getFile();
        }
        if (rf.getFile().extension() == ".hpch")
        {
            if (provided_pchh)
                throw SW_RUNTIME_ERROR("More than one pch passed");
            provided_pchh = rf.getFile();
        }
    }

    // more setup
    auto vs_setup = [this, nt, &provided_pdb, &provided_pch](auto *c)
    {
        // set pdb explicitly
        // this is needed when using pch files sometimes
        if (provided_pdb)
            c->PDBFilename = *provided_pdb;
        else if (!nt->pch.pch.empty())
            c->PDBFilename = nt->pch.pdb;
        else
            c->PDBFilename = nt->BinaryDir.parent_path() / "obj" / "sw.pdb";
        c->getCommand()->simultaneous_outputs.insert(c->PDBFilename());

        if (provided_pch && !nt->pch.name.empty())
        {
            throw SW_RUNTIME_ERROR(nt->getPackage().toString() + "You have two pchs: provided (" +
                to_printable_string(*provided_pch) + ") and from current target");
        }
    };
    if (auto c = cl.as<VisualStudioCompiler*>())
    {
        vs_setup(c);
    }
    else if (auto c = cl.as<ClangClCompiler *>())
    {
        vs_setup(c);
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
        auto writef = [nt, &fidx, &rfs_unity](auto &d)
        {
            if (d.s.empty())
                return;
            auto fns = "Module." + std::to_string(fidx++) + d.ext;
            auto fn = nt->BinaryPrivateDir / "unity" / fns;
            write_file_if_different(fn, d.s); // do not trigger rebuilds
            //getMergeObject()[fn].fancy_name = "[" + getPackage().toString() + "]/[unity]/" + fns;
            d.s.clear();
            rfs_unity.addFile(fn);
        };

        for (auto &[n,rf] : rfs)
        {
            // skip when args are populated
            if (!rf.getAdditionalArguments().empty())
            {
                auto &rf2 = rfs_unity.addFile(rf.getFile());
                SW_UNIMPLEMENTED;
                rf2 = rf; // what about deps?
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
    for (auto &[fn,rf] : rfs_unity.empty() ? rfs : rfs_unity)
    {
        if (!exts.contains(rf.getFile().extension().string()))
            continue;
        const auto output = getOutputFile(t, rf.getFile());
        if (rfs.contains(output))
            continue;

        auto c = cl.clone();
        auto &nc = static_cast<NativeCompiler &>(*c);
        nc.setSourceFile(rf.getFile(), output);

        // pch
        if (rf.getFile() == nt->pch.source)
        {
            auto setup_vs = [nt](auto C)
            {
                C->CreatePrecompiledHeader = nt->pch.header;
                C->PrecompiledHeaderFilename = nt->pch.pch;
                C->PrecompiledHeaderFilename.output_dependency = true;
            };
            auto setup_gnu = [nt, &nc, &rfs, &output](auto C)
            {
                C->Language = "c++-header";

                // set new input and output
                nc.setSourceFile(nt->pch.header, nt->pch.pch);

                // skip .obj output
                rfs.erase(output);

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
                setup_gnu(C);
            else if (auto C = c->as<GNUCompiler *>())
                setup_gnu(C);
            else
                SW_UNIMPLEMENTED;
        }
        else if (!nt->pch.name.empty() || provided_pch)
        {
            auto setup_vs = [nt, &provided_pch, &provided_pchh](auto C)
            {
                if (provided_pch)
                {
                    if (!provided_pchh)
                        throw SW_RUNTIME_ERROR("hpch was not provided");
                    auto fn = provided_pchh->parent_path() / provided_pchh->stem();
                    C->UsePrecompiledHeader = fn;
                    C->PrecompiledHeaderFilename = *provided_pch;
                    C->ForcedIncludeFiles().insert(C->ForcedIncludeFiles().begin(), fn);
                    // must add manually
                    for (auto &fi : nt->getMergeObject().ForceIncludes)
                        C->ForcedIncludeFiles().push_back(fi);
                }
                else
                {
                    C->UsePrecompiledHeader = nt->pch.header;
                    C->PrecompiledHeaderFilename = nt->pch.pch;
                }
                C->PrecompiledHeaderFilename.input_dependency = true;
            };
            auto setup_gnu = [](auto C, auto ext)
            {
                // we must add this explicitly
                SW_UNIMPLEMENTED;
                //C->createCommand(t.getMainBuild())->addInput(path(*pch_basename) += ".h"s += ext);
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
                SW_UNIMPLEMENTED;
                /*auto c = rf.getFile().extension() == ".c";

                pp_command.PreprocessToFile = true;
                pp_command.PreprocessFileName = base_command.getOutputFile() += (c ? ".i" : ".ii");
                pp_command.Output.clear();
                base_command.setSourceFile(pp_command.PreprocessFileName(), base_command.getOutputFile());*/
            };
            auto gnu_setup = [](NativeCompiler &base_command, auto &pp_command)
            {
                SW_UNIMPLEMENTED;
                // set pp
                /*pp_command.CompileWithoutLinking = false;
                pp_command.Preprocess = true;
                auto o = pp_command.getOutputFile();
                o = o.parent_path() / o.stem() += ".i";
                pp_command.setOutputFile(o);

                // set input file for old command
                base_command.setSourceFile(pp_command.getOutputFile(), base_command.getOutputFile());*/
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
            SW_UNIMPLEMENTED;
            //commands.emplace(pp_command->getCommand());
        }

        nc.prepareCommand(t);
        nc.getCommand()->push_back(arguments);
        if (!rf.getAdditionalArguments().empty())
        {
            switch (lang)
            {
            case LANG_ASM:
                nc.getCommand()->push_back(rf.getAdditionalArguments()["asm"]);
                break;
            case LANG_C:
                nc.getCommand()->push_back(rf.getAdditionalArguments()["c"]);
                break;
            case LANG_CPP:
                nc.getCommand()->push_back(rf.getAdditionalArguments()["cpp"]);
                break;
            }
        }
        /*nc.getCommand()->name = rulename;
        if (!rulename.empty())
            nc.getCommand()->name += " ";*/
        nc.getCommand()->name += "[" + t.getPackage().toString() + "]" + tfns.getName(rf.getFile());
        auto &rf = rfs.addFile(output);
        rf.setCommand(c->getCommand());
        rf.addDependency(fn);
    }
}

void NativeLinkerRule::setup(const Target &t)
{
    auto &prog_link = static_cast<NativeLinker &>(*program);
    auto nt = t.as<NativeCompiledTarget *>();
    if (!nt)
        return;

    if (!is_linker)
    {
        prog_link.setOutputFile(nt->getOutputFileName2("lib") += nt->getBuildSettings().TargetOS.getStaticLibraryExtension());
    }
    else
    {
        auto ext = nt->getBuildSettings().TargetOS.getExecutableExtension();
        if (nt->isExecutable())
        {
            prog_link.Prefix.clear();
            //prog_link.Extension =
            if (auto c = prog_link.as<VisualStudioLinker *>())
            {
                c->ImportLibrary.output_dependency = false; // become optional
                c->ImportLibrary.create_directory = true; // but create always
            }
            else if (auto L = prog_link.as<GNULinker *>())
            {
                L->PositionIndependentCode = false;
                L->SharedObject = false;
            }
        }
        else
        {
            ext = nt->getBuildSettings().TargetOS.getSharedLibraryExtension();
            if (prog_link.Type == LinkerType::MSVC)
            {
                // set machine to target os arch
                auto L = prog_link.as<VisualStudioLinker *>();
                L->Dll = true;
            }
            else if (prog_link.Type == LinkerType::GNU)
            {
                auto L = prog_link.as<GNULinker *>();
                L->SharedObject = true;
                if (nt->getBuildSettings().TargetOS.Type == OSType::Linux)
                    L->AsNeeded = true;
            }
        }

        prog_link.setOutputFile(nt->getOutputFileName2("bin") += ext);
        prog_link.setImportLibrary(nt->getOutputFileName2("lib"));

        if (auto L = prog_link.as<VisualStudioLibraryTool *>())
        {
            L->NoDefaultLib = true;
        }
        if (auto L = prog_link.as<VisualStudioLinker *>())
        {
            if (!L->GenerateDebugInformation)
            {
                if (nt->getBuildSettings().Native.ConfigurationType == ConfigurationType::Debug ||
                    nt->getBuildSettings().Native.ConfigurationType == ConfigurationType::ReleaseWithDebugInformation)
                {
                    //if (auto g = getSolution().getGenerator(); g && g->type == GeneratorType::VisualStudio)
                    //c->GenerateDebugInformation = vs::link::Debug::FastLink;
                    //else
                    L->GenerateDebugInformation = vs::link::Debug::Full;
                }
                else
                    L->GenerateDebugInformation = vs::link::Debug::None;
            }

            if (L->GenerateDebugInformation() != vs::link::Debug::None && !L->PDBFilename)
            {
                auto f = nt->getOutputFile();
                f = f.parent_path() / f.filename().stem();
                f += ".pdb";
                L->PDBFilename = f;// BinaryDir.parent_path() / "obj" / (getPackage().getPath().toString() + ".pdb");
            }
            else
                L->PDBFilename.output_dependency = false;
        }
    }

    // at last
    prog_link.merge(nt->getMergeObject());
}

void NativeLinkerRule::addInputs(const Target &t, RuleFiles &rfs)
{
    auto &nl = static_cast<NativeLinker &>(*program);
    auto nt = t.as<NativeCompiledTarget *>();

    std::optional<path> def;
    FilesOrdered files;
    for (auto &[_,rf] : rfs)
    {
        //if (used_files.contains(rf))
            //continue;

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

        files.push_back(rf.getFile());
    }
    std::sort(files.begin(), files.end());
    if (files.empty() && !def)
        return;

    // objs must go into object files
    // libs into library deps
    //getSelectedTool()->setObjectFiles(files);
    //getSelectedTool()->setInputLibraryDependencies(gatherLinkLibraries());

    auto c = nl.clone();
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
            auto c = std::make_shared<builder::BuiltinCommand>(t.getMainBuild(), SW_VISIBLE_BUILTIN_FUNCTION(create_def_file));
            c->push_back(deffn);
            c->addOutput(deffn);
            c->push_back(objs);
            c->addInput(objs);
            def = deffn;
            //command_lib = c;
            auto &rf = rfs.addFile(deffn);
            rf.setCommand(c);
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
                for (auto &l : VSL->System.LinkLibraries)
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
    files.erase(std::remove(files.begin(), files.end(), nt->getImportLibrary()), files.end());
    nc.setObjectFiles(files);
    nc.prepareCommand(t);
    nc.getCommand()->push_back(arguments);
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
    //if (!rfs.emplace(nc.getOutputFile(), nc.getOutputFile()).second)
        //return;
    //used_files.insert(nc.getOutputFile());
    c->getCommand()->prepare(); // why?
    c->getCommand()->name = //(is_linker ? "[LINK]"s : "[LIB]"s) + " " +
        "[" + t.getPackage().toString() + "]" + nt->getOutputFile().extension().string();
    //nt->registerCommand(*c->getCommand());
    //command = c->getCommand();
    auto &rf = rfs.addFile(nc.getOutputFile());
    rf.resetCommand(c->getCommand());
}

void RcRule::setup(const Target &t)
{
}

RuleFiles RcRule::addInput(const Target &t, const RuleFiles &rfs, RuleFile &rf)
{
    if (rf.getFile().extension() != ".rc")
        return {};

    auto output = getOutputFileBase(t, rf.getFile()) += ".res";
    if (rfs.contains(output))
        return {};

    auto c = program->clone();
    auto &rc = static_cast<RcTool &>(*c);
    // add casual idirs?
    rc.InputFile = rf.getFile();
    rc.Output = output;
    rc.prepareCommand(t);
    rc.getCommand()->push_back(arguments);

    RuleFiles rfs_new;
    auto &rf2 = rfs_new.addFile(output);
    rf2.setCommand(c->getCommand());
    return rfs_new;
}

}
