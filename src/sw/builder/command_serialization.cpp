// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2020 Egor Pugin

#include "command.h"

#include "command_storage.h"

#include <sw/protocol/build.pb.h>

#include <primitives/exceptions.h>

#include <fstream>

namespace sw
{

static api::build::Command saveCommand(const builder::Command &in)
{
    api::build::Command c;
    c.set_id((size_t)&in);
    if (in.command_storage)
        c.set_command_storage_root(to_printable_string(in.command_storage->root));
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
#define SERIALIZE_FILES(x) \
    for (auto &i : in.x)   \
    c.mutable_##x()->Add(to_printable_string(i))
    SERIALIZE_FILES(inputs);
    SERIALIZE_FILES(outputs);
    SERIALIZE_FILES(implicit_inputs);
    SERIALIZE_FILES(simultaneous_outputs);
    SERIALIZE_FILES(inputs_without_timestamps);
#undef SERIALIZE_FILES
    for (auto &d : in.getDependencies())
        c.mutable_dependencies()->Add((size_t)d);
    return c;
}

static std::pair<size_t, std::shared_ptr<builder::Command>> loadCommand(const api::build::Command &in)
{
    auto c = std::make_shared<builder::Command>();
    c->command_storage_root = in.command_storage_root();
    c->working_directory = in.working_directory();
    for (auto &a : in.arguments())
        c->push_back(a);
    for (auto &[k, v] : in.environment())
        c->environment[k] = v;
    c->in.file = in.in().file();
    c->in.text = in.in().text();
    c->out.file = in.out().file();
    c->out.text = in.out().text();
    c->err.file = in.err().file();
    c->err.text = in.err().text();
#define SERIALIZE_FILES(x) \
    for (auto &i : in.x()) \
    c->x.insert(i)
    SERIALIZE_FILES(inputs);
    SERIALIZE_FILES(outputs);
    SERIALIZE_FILES(implicit_inputs);
    SERIALIZE_FILES(simultaneous_outputs);
    SERIALIZE_FILES(inputs_without_timestamps);
#undef SERIALIZE_FILES
    for (auto &a : in.dependencies())
        c->addDependency(*(CommandNode*)a);
    return { in.id(), c };
}

void saveCommands(const path &p, const Commands &commands, int type)
{
    fs::create_directories(p.parent_path());

    api::build::Commands cmds;
    for (auto &c : commands)
        cmds.mutable_commands()->Add(saveCommand(*c));

    std::ofstream ofs(p);
    if (!ofs)
        throw SW_RUNTIME_ERROR("Cannot write file: " + to_string(p));
    cmds.SerializeToOstream(&ofs);
}

Commands loadCommands(const path &p, int type)
{
    std::ifstream ifs(p);
    if (!ifs)
        throw SW_RUNTIME_ERROR("Cannot open file: " + to_string(p));
    api::build::Commands commands;
    commands.ParseFromIstream(&ifs);

    Commands cmds;
    for (auto &c : commands.commands())
        cmds.insert(loadCommand(c).second);
    return cmds;
}

}
