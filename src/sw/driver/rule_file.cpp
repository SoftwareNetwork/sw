// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#include "rule_file.h"

#include <primitives/exceptions.h>

namespace sw
{

void RuleFile::setCommand(const std::shared_ptr<builder::Command> &c)
{
    if (command)
        throw SW_RUNTIME_ERROR("Setting output command twice for file: " + to_printable_string(normalize_path(getFile())));
    resetCommand(c);
}

void RuleFile::resetCommand(const std::shared_ptr<builder::Command> &c)
{
    command = c;
}

void RuleFile::addDependency(const path &fn)
{
    if (fn == file)
        throw SW_RUNTIME_ERROR("Adding self dependency: " + to_printable_string(normalize_path(fn)));
    dependencies.insert(fn);
}

std::shared_ptr<builder::Command> RuleFile::getCommand(const RuleFiles &rfs) const
{
    if (!command)
        return {};
    DEBUG_BREAK_IF(file.filename().string().find("qtimer") != -1);
    auto deps = getDependencies1(rfs);
    for (auto &d : deps)
        command->addDependency(*d);
    return command;
}

Commands RuleFile::getDependencies1(const RuleFiles &rfs) const
{
    Commands cmds;
    for (auto &d : getDependencies())
    {
        bool processed = false;
        // process normal deps (files)
        auto i = rfs.rfs.find(d);
        if (i != rfs.rfs.end())
        {
            if (i->second.command)
                cmds.insert(i->second.command);
            else
                // no stack overflow guard atm
                cmds.merge(i->second.getDependencies1(rfs));
            processed = true;
        }
        // process free deps (files)
        if (auto i = rfs.commands.find(d); i != rfs.commands.end())
        {
            cmds.insert(i->second);
            processed = true;
        }
        if (!processed)
            throw SW_RUNTIME_ERROR("Dependency was set on file '" + to_printable_string(normalize_path(d)) + "', but not added to rule files");
    }
    return cmds;
}

RuleFile &RuleFiles::addFile(const path &p)
{
    auto [i, _] = rfs.emplace(p, p);
    return i->second;
}

void RuleFiles::merge(RuleFiles &rhs)
{
    rfs.merge(rhs.rfs);
}

void RuleFiles::addCommand(const path &output, const std::shared_ptr<builder::Command> &c)
{
    if (!commands.emplace(output, c).second)
        throw SW_RUNTIME_ERROR("Setting output command twice for file: " + to_printable_string(normalize_path(output)));
}

Commands RuleFiles::getCommands() const
{
    Commands cmds;
    for (auto &[p, c] : commands)
        cmds.insert(c);
    for (auto &[_, rf] : rfs)
    {
        if (auto c = rf.getCommand(*this))
            cmds.insert(c);
    }
    // set deps, naive way
    /*for (auto &[_, rf] : rfs)
    {
        // only for non-generated files
        // like original .cpp -> .obj
        // actually we must set dependency to .obj, but we cannot do that directly,
        // since we do not have outputs (generated) list
        //if (rf.command)
            //continue;
        for (auto &d : rf.getDependencies())
        {
            for (auto &c : cmds)
            {
                if (c->inputs.contains(rf.getFile()))
                {
                    if (c.get() != d)
                        c->addDependency(*d);
                }
            }
        }
    }*/
    return cmds;
}

/*Commands RuleFiles::getDependencies(const RuleFile &rf) const
{
    Commands cmds;
    for (auto &p : rf.dependencies)
    {
        auto i = rfs.find(p);
        if (i == rfs.end())
            throw SW_RUNTIME_ERROR();
    }
    return cmds;
}*/

} // namespace sw
