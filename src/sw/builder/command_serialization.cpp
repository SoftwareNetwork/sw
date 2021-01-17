// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin

#include "command.h"

#include <sw/protocol/build.pb.h>

#include <primitives/exceptions.h>

#include <fstream>

namespace sw
{

static api::build::Command saveCommand(const builder::Command &in)
{
    api::build::Command c;
    c.set_id((size_t)&in);
    c.set_working_directory(to_printable_string(in.working_directory));
    for (auto &a : in.arguments)
        c.mutable_arguments()->Add(a->toString());
    for (auto &[k,v] : in.environment)
        (*c.mutable_environment())[k] = v;
    c.mutable_in()->set_file(to_printable_string(in.in.file));
    c.mutable_in()->set_text(in.in.text);
    c.mutable_out()->set_file(to_printable_string(in.out.file));
    c.mutable_out()->set_text(in.out.text);
    c.mutable_err()->set_file(to_printable_string(in.err.file));
    c.mutable_err()->set_text(in.err.text);
    for (auto &d : in.getDependencies())
        c.mutable_dependencies()->Add((size_t)d);
    return c;
}

static api::build::Commands saveCommands(const Commands &commands)
{
    api::build::Commands cmds;
    for (auto &c : commands)
        cmds.mutable_commands()->Add(saveCommand(*c));
    return cmds;
}

void saveCommands(const path &p, const Commands &commands, int type)
{
    fs::create_directories(p.parent_path());

    auto cmds = saveCommands(commands);

    std::ofstream ofs(p);
    if (!ofs)
        throw SW_RUNTIME_ERROR("Cannot write file: " + to_string(p));
    cmds.SerializeToOstream(&ofs);
}

}
