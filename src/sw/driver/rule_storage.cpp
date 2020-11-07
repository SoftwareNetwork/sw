// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#include "rule_storage.h"

#include "dependency.h"
#include "program.h"
#include "target/base.h"

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "rule_storage");

namespace sw
{

RuleSystem::RuleDescription::RuleDescription(const String &name, const DependencyPtr &from_dep, const String &from_name)
{
    rule_name = name;
    dep = from_dep;
    target_rule_name = from_name;
}

RuleSystem::RuleDescription::RuleDescription(const String &name, const DependencyPtr &from_dep)
    : RuleDescription(name, from_dep, name)
{
}

RuleSystem::RuleDescription::RuleDescription(const String &name, const UnresolvedPackage &from_dep)
    : RuleDescription(name, std::make_shared<Dependency>(from_dep))
{
}

RuleSystem::RuleDescription::RuleDescription(const String &name, const TargetSettings &ts)
    : RuleDescription(name, ts["rule"][name]["package"].getValue())
{
}

IRule &RuleSystem::RuleDescription::getRule() const
{
    if (ptr)
        return *ptr;
    auto t = dep->getTarget().as<PredefinedProgram *>();
    if (!t)
        SW_UNIMPLEMENTED;
    ptr = t->getRule1(target_rule_name);
    return *ptr;
}

void RuleSystem::addRuleDependency(const RuleDescription &d, bool overwrite)
{
    auto [i,inserted] = rule_dependencies.emplace(d.rule_name, d);
    if (inserted)
        return;
    if (!overwrite)
        LOG_DEBUG(logger, "Overridding rule '" + d.rule_name);
        //throw SW_RUNTIME_ERROR("Trying to set rule '" + d.rule_name + "' for the second time");
    i->second = d;
}

void RuleSystem::addRuleDependency(const String &name, const DependencyPtr &from_dep, const String &from_name)
{
    addRuleDependency({ name, from_dep, name });
}

void RuleSystem::addRuleDependency(const String &name, const DependencyPtr &from_dep)
{
    addRuleDependency(name, from_dep, name);
}

DependencyPtr RuleSystem::getRuleDependency(const String &name) const
{
    auto i = rule_dependencies.find(name);
    if (i == rule_dependencies.end())
        throw SW_RUNTIME_ERROR("No rule dep: " + name);
    return i->second.dep;
}

IRulePtr RuleSystem::getRuleFromDependency(const String &ruledepname, const String &rulename) const
{
    auto dep = getRuleDependency(ruledepname);
    if (auto t = dep->getTarget().as<PredefinedProgram *>())
        return t->getRule1(rulename);
    else
        SW_UNIMPLEMENTED;
}

IRulePtr RuleSystem::getRuleFromDependency(const String &rulename) const
{
    return getRuleFromDependency(rulename, rulename);
}

std::vector<IDependency *> RuleSystem::getRuleDependencies() const
{
    std::vector<IDependency *> r;
    for (auto &[_, rd] : rule_dependencies)
        r.push_back(rd.dep.get());
    return r;
}

void RuleSystem::runRules(RuleFiles rfs, const Target &t)
{
    for (auto &[_,rd] : rule_dependencies)
    {
        auto nr = dynamic_cast<NativeRule*>(&rd.getRule());
        if (!nr)
            continue;
        nr->arguments.push_back(rule_properties[rd.rule_name].getArguments());
        nr->setup(t);
    }
    while (1)
    {
        bool newfile = false;
        for (auto &[_,rd] : rule_dependencies)
        {
            auto nr = dynamic_cast<NativeRule*>(&rd.getRule());
            if (!nr)
                continue;
            auto outputs = nr->addInputs(t, rfs);
            for (auto &o : outputs)
            {
                auto [_, inserted] = rfs.insert(o);
                newfile |= inserted;
            }
        }
        if (!newfile)
            break;
    }
}

Commands RuleSystem::getRuleCommands() const
{
    Commands c;
    for (auto &[_,rd] : rule_dependencies)
        c.merge(rd.getRule().getCommands());
    return c;
}

} // namespace sw
