// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "base.h"

#include "sw/driver/compiler/detect.h"

#include <sw/core/build.h>

namespace sw
{

template <class CompilerType>
static std::shared_ptr<CompilerType> activateCompiler(Target &t, const UnresolvedPackage &id, const StringSet &exts)
{
    auto &cld = t.getMainBuild().getTargets();

    TargetSettings oss; // empty for now
    auto i = cld.find(id, oss);
    if (!i)
    {
        i = t.getContext().getPredefinedTargets().find(id, oss);
        if (!i)
        {
            SW_UNIMPLEMENTED;
            //for (auto &e : exts)
                //t.setExtensionProgram(e, id);
            return {};
        }
    }
    auto prog = i->as<PredefinedProgram *>();
    if (!prog)
        throw SW_RUNTIME_ERROR("Target without PredefinedProgram: " + i->getPackage().toString());

    auto set_compiler_type = [&t, &id, &exts](const auto &c)
    {
        SW_UNIMPLEMENTED;
        //for (auto &e : exts)
            //t.setExtensionProgram(e, c->clone());
    };

    auto c1 = prog->getProgram().clone();
    if (auto c = dynamic_cast<CompilerBaseProgram*>(c1.get()))
    {
        set_compiler_type(c);
        return {};
    }

    bool created = false;
    auto create_command = [&prog, &created, &t](auto &c)
    {
        if (created)
            return;
        c->file = prog->getProgram().file;
        auto C = c->createCommand(t.getMainBuild());
        static_cast<primitives::Command&>(*C) = *prog->getProgram().getCommand();
        created = true;
    };

    auto compiler = std::make_shared<CompilerType>();
    create_command(compiler);
    set_compiler_type(compiler);
    return compiler;
}

}
