// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#include "rule_file.h"

namespace sw
{

RuleFile &RuleFiles::addFile(const RuleFile &rf)
{
    auto [i, _] = rfs.insert_or_assign(rf.getFile(), rf);
    return i->second;
}

void RuleFiles::merge(RuleFiles &rhs)
{
    rfs.merge(rhs.rfs);
}

void RuleFiles::addCommand(const std::shared_ptr<builder::Command> &c)
{
    commands.insert(c);
}

Commands RuleFiles::getCommands() const
{
    auto cmds = commands;
    for (auto &[_, rf] : rfs)
    {
        if (rf.command)
            cmds.insert(rf.command);
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
