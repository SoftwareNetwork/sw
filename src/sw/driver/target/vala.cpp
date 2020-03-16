/*
 * SW - Build System and Package Manager
 * Copyright (C) 2020 Egor Pugin
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

void ValaBase::init()
{
    auto &t = dynamic_cast<NativeCompiledTarget &>(*this);

    t.add(CallbackType::CreateTargetInitialized, [&t]()
    {
        if (t.getType() == TargetType::NativeSharedLibrary)
            t.ExportAllSymbols = true;
        if (t.getBuildSettings().Native.LibrariesType == LibraryType::Shared &&
            t.getType() == TargetType::NativeLibrary)
            t.ExportAllSymbols = true;
    });

    d = "org.sw.demo.gnome.vala.compiler"_dep;
    d->getSettings() = t.getSettings();
    // glib+gobject currently do not work in other configs
    d->getSettings()["native"]["library"] = "shared";
    d->getSettings()["native"]["configuration"] = "debug";
    t.setExtensionProgram(".vala", d);
    t += "org.sw.demo.gnome.glib.glib"_dep;
}

void ValaBase::prepare()
{
    auto &t = dynamic_cast<NativeCompiledTarget &>(*this);

    compiler = std::make_shared<ValaCompiler>();
    auto &dt = d->getTarget();
    if (auto t2 = dt.as<ExecutableTarget *>())
    {
        compiler->file = t2->getOutputFile();
        auto c = compiler->createCommand(t.getMainBuild());
        t2->setupCommand(*c);
    }
    else
        SW_UNIMPLEMENTED;

    compiler->OutputDir = t.BinaryDir.parent_path() / "obj";

    compiler->InputFiles = {};
    for (auto &f : ::sw::gatherSourceFiles<SourceFile>(t, {".vala"}))
    {
        auto rel = f->file.lexically_relative(t.SourceDir);
        auto o = compiler->OutputDir() / rel.parent_path() / rel.stem() += ".c";
        auto c = compiler->createCommand(t.getMainBuild());
        File(o, t.getFs()).setGenerator(c, false);
        t += o;
        c->addOutput(o);

        compiler->InputFiles().push_back(f->file);
        f->skip = true;
    }
}

void ValaBase::getCommands(Commands &cmds) const
{
    auto c = compiler->getCommand(dynamic_cast<const Target &>(*this));
    c->use_response_files = false;
    cmds.insert(c);
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
        ValaBase::getCommands(cmds);      \
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
