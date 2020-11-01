// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#include "vala.h"

#include "../build.h"
#include "../command.h"
#include "../source_file.h"
#include "../suffix.h"
#include "../compiler/compiler_helpers.h"

namespace sw
{

namespace detail
{

ValaBase::~ValaBase()
{
}

path ValaBase::getOutputCCodeFileName(const path &f) const
{
    auto rel = f.lexically_relative(t_->SourceDir);
    auto o = OutputDir / rel.parent_path() / rel.stem() += ".c";
    return o;
}

void ValaBase::init()
{
    t_ = dynamic_cast<NativeCompiledTarget*>(this);
    auto &t = *t_;

    t.add(CallbackType::CreateTarget, [this, &t]()
    {
        if (t.getType() == TargetType::NativeSharedLibrary)
            t.ExportAllSymbols = true;
        if (t.getBuildSettings().Native.LibrariesType == LibraryType::Shared &&
            t.getType() == TargetType::NativeLibrary)
            t.ExportAllSymbols = true;

        if (t.getType() != TargetType::NativeExecutable)
        {
            t.Interface.CustomTargetOptions[VALA_OPTIONS_NAME]
                .push_back(to_string(normalize_path(t.BinaryDir.parent_path() / "obj" / t.getPackage().toString() += ".vapi")));
            t.Interface.IncludeDirectories.push_back(t.BinaryDir.parent_path() / "obj");
        }

        // must add only after native target init
        // because of unresolved native programs before that
        d = "org.sw.demo.gnome.vala.compiler"_dep;
        //d->getSettings() = t.getSettings();
        // glib+gobject currently do not work in other configs
        d->getSettings()["native"]["library"] = "shared";
        d->getSettings()["native"]["configuration"] = "debug";
        SW_UNIMPLEMENTED;
        //t.setExtensionProgram(".vala", d);
        //t += "org.sw.demo.gnome.glib.glib"_dep;
        t += "org.sw.demo.gnome.glib.gobject"_dep;
        //t += "org.sw.demo.gnome.glib.gmodule"_dep;
        //"--profile=posix" removes need in glib dependency

        OutputDir = t.BinaryDir.parent_path() / "obj";
    });
}

void ValaBase::prepare()
{
    auto &t = *t_;

    SW_UNIMPLEMENTED;
    //compiler = std::make_shared<ValaCompiler>();
    //auto &dt = d->getTarget();
    /*if (auto t2 = dt.as<ExecutableTarget *>())
    {
        compiler->file = t2->getOutputFile();
        auto c = compiler->createCommand(t.getMainBuild());
        t2->setupCommand(*c);
    }
    else
        SW_UNIMPLEMENTED;

    auto c = compiler->createCommand(t.getMainBuild());
    compiler->OutputDir = OutputDir;
    compiler->InputFiles = FilesOrdered{};
    for (auto &f : ::sw::gatherSourceFiles<SourceFile>(t, {".vala"}))
    {
        auto o = getOutputCCodeFileName(f->file);
        File(o, t.getFs()).setGenerator(c, false);
        t += o;
        c->addOutput(o);

        compiler->InputFiles().push_back(f->file);
        f->skip = true;
    }

    // #line directives
    if (t.getBuildSettings().Native.ConfigurationType != ConfigurationType::Release)
        c->push_back("-g");

    if (t.getType() != TargetType::NativeExecutable)
    {
        auto h = compiler->OutputDir() / t.getPackage().getPath().toString() += ".h";
        c->push_back("-H");
        c->push_back(h);
        c->push_back("--library");
        c->push_back(t.getPackage().toString());
        c->addOutput(compiler->OutputDir() / t.getPackage().toString() += ".vapi");
        c->addOutput(h);
    }

    if (auto i = t.CustomTargetOptions.find(VALA_OPTIONS_NAME); i != t.CustomTargetOptions.end())
    {
        for (auto &o : i->second)
            c->push_back(o);
    }*/
}

void ValaBase::getCommands1(Commands &cmds) const
{
    SW_UNIMPLEMENTED;
    /*auto c = compiler->getCommand(dynamic_cast<const Target &>(*this));
    c->use_response_files = false;
    cmds.insert(c);*/
}

}

#define VALA_INIT(t, ...)     \
    bool t::init()            \
    {                         \
        switch (init_pass)    \
        {                     \
        case 1:               \
            Target::init();   \
            ValaBase::init(); \
            break;            \
        }                     \
                              \
        return Base::init();  \
    }

#define VALA_PREPARE(t)          \
    bool t::prepare()            \
    {                            \
        switch (prepare_pass)    \
        {                        \
        case 5:                  \
        {                        \
            ValaBase::prepare(); \
            break;               \
        }                        \
        }                        \
                                 \
        return Base::prepare();  \
    }

#define VALA_GET_COMMANDS(t)              \
    Commands t::getCommands1() const      \
    {                                     \
        auto cmds = Base::getCommands1(); \
        ValaBase::getCommands1(cmds);     \
        return cmds;                      \
    }

VALA_INIT(ValaLibrary)
VALA_PREPARE(ValaLibrary)
VALA_GET_COMMANDS(ValaLibrary)

VALA_INIT(ValaStaticLibrary)
VALA_PREPARE(ValaStaticLibrary)
VALA_GET_COMMANDS(ValaStaticLibrary)

VALA_INIT(ValaSharedLibrary)
VALA_PREPARE(ValaSharedLibrary)
VALA_GET_COMMANDS(ValaSharedLibrary)

VALA_INIT(ValaExecutable)
VALA_PREPARE(ValaExecutable)
VALA_GET_COMMANDS(ValaExecutable)

}
