// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#include "target2.h"

#include "../rule.h"

#include <sw/core/build.h>
#include <sw/core/sw_context.h>

namespace sw
{

struct MsvcRule : IRule
{
    Commands getCommands() const override
    {
        return {};
    }
};

/*std::unique_ptr<IRule> PredefinedTargetWithRule::getRule() const
{
    return std::make_unique<MsvcRule>();
}*/

Target2::Target2(TargetBase &parent, const PackageId &id)
    : Target(parent, id), NativeTargetOptionsGroup((Target &)*this)
{

}

Commands Target2::getCommands1() const
{
    /*auto it = getMainBuild().getContext().getPredefinedTargets().find(UnresolvedPackage{ "msvc" });
    if (it == getMainBuild().getContext().getPredefinedTargets().end())
        throw SW_RUNTIME_ERROR("no rule found");
    if (it->second.empty())
        throw SW_RUNTIME_ERROR("no rules inside pkg");*/

    /*auto r = (*it->second.begin())->getRule();
    if (!r)
        throw SW_RUNTIME_ERROR("empty rule");

    return r->getCommands();*/
    SW_UNIMPLEMENTED;
}

} // namespace sw
