// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#include "rule_file.h"

#include <primitives/exceptions.h>

namespace sw
{

void RuleFile::setCommand(const std::shared_ptr<builder::Command> &c)
{
    if (command)
        throw SW_RUNTIME_ERROR("Setting output command twice for file: " + to_printable_string(normalize_path(getFile())));
    command = c;
}

RuleFile &RuleFiles::addFile(const RuleFile &rf)
{
    auto [i, _] = rfs.insert_or_assign(rf.getFile(), rf);
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
        if (auto c = rf.getCommand())
            cmds.insert(c);
    }
    // set deps, naive way
    for (auto &[_, rf] : rfs)
    {
        // only for non-generated files
        // like original .cpp -> .obj
        // actually we must set dependency to .obj, but we cannot do that directly,
        // since we do not have outputs (generated) list
        //if (rf.command)
            //continue;
        for (auto &d : rf.dependencies)
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
    }
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
