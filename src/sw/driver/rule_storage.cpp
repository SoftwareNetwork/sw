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

void RuleStorage::push(const String &name, IRulePtr r)
{
    RuleData rd;
    rd.rule = std::move(r);
    if (!rules[name].empty())
    {
        auto nr = dynamic_cast<NativeRule *>(rd.rule.get());
        if (nr)
            nr->arguments.push_back(rules[name].top().getArguments());
    }
    rules[name].emplace(std::move(rd));
}

IRulePtr RuleStorage::pop(const String &name)
{
    if (!contains(name))
        return {};
    IRulePtr r = std::move(rules[name].top().rule);
    rules[name].pop();
    return r;
}

bool RuleStorage::contains(const String &name) const
{
    return rules.contains(name);
}

void RuleStorage::clear()
{
    rules.clear();
}

void RuleStorage::clear(const String &name)
{
    rules.erase(name);
}

Commands RuleStorage::getCommands() const
{
    Commands c;
    for (auto &[_,s] : rules)
        c.merge(s.top().rule->getCommands());
    return c;
}

RuleStorage::RuleData &RuleStorage::getRule(const String &n)
{
    if (rules[n].empty())
        rules[n].push({});
    return rules[n].top();
}

const RuleStorage::RuleData &RuleStorage::getRule(const String &n) const
{
    auto i = rules.find(n);
    if (i == rules.end())
        throw SW_RUNTIME_ERROR("No such rule: " + n);
    return i->second.top();
}

Commands RuleSystem::getRuleCommands() const
{
    return rules.getCommands();
}

void RuleSystem::runRules(RuleFiles rfs, const Target &t)
{
    for (auto &r : rules)
    {
        auto nr = dynamic_cast<NativeRule*>(r.rule.get());
        if (!nr)
            continue;
        nr->setup(t);
    }
    while (1)
    {
        bool newfile = false;
        for (auto &r : rules)
        {
            auto nr = dynamic_cast<NativeRule*>(r.rule.get());
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

RuleSystem2::RuleDescription::RuleDescription(const String &name, const DependencyPtr &from_dep, const String &from_name)
{
    rule_name = name;
    dep = from_dep;
    target_rule_name = from_name;
}

RuleSystem2::RuleDescription::RuleDescription(const String &name, const DependencyPtr &from_dep)
    : RuleDescription(name, from_dep, name)
{
}

RuleSystem2::RuleDescription::RuleDescription(const String &name, const UnresolvedPackage &from_dep)
    : RuleDescription(name, std::make_shared<Dependency>(from_dep))
{
}

RuleSystem2::RuleDescription::RuleDescription(const String &name, const TargetSettings &ts)
    : RuleDescription(name, ts["rule"][name]["package"].getValue())
{
}

IRule &RuleSystem2::RuleDescription::getRule() const
{
    if (ptr)
        return *ptr;
    auto t = dep->getTarget().as<PredefinedProgram *>();
    if (!t)
        SW_UNIMPLEMENTED;
    ptr = t->getRule1(target_rule_name);
    return *ptr;
}

void RuleSystem2::addRuleDependency(const RuleDescription &d, bool overwrite)
{
    auto [i,inserted] = rule_dependencies.emplace(d.rule_name, d);
    if (inserted)
        return;
    if (!overwrite)
        LOG_DEBUG(logger, "Overridding rule '" + d.rule_name);
        //throw SW_RUNTIME_ERROR("Trying to set rule '" + d.rule_name + "' for the second time");
    i->second = d;
}

void RuleSystem2::addRuleDependency(const String &name, const DependencyPtr &from_dep, const String &from_name)
{
    addRuleDependency({ name, from_dep, name });
}

void RuleSystem2::addRuleDependency(const String &name, const DependencyPtr &from_dep)
{
    addRuleDependency(name, from_dep, name);
}

DependencyPtr RuleSystem2::getRuleDependency(const String &name) const
{
    auto i = rule_dependencies.find(name);
    if (i == rule_dependencies.end())
        throw SW_RUNTIME_ERROR("No rule dep: " + name);
    return i->second.dep;
}

IRulePtr RuleSystem2::getRuleFromDependency(const String &ruledepname, const String &rulename) const
{
    auto dep = getRuleDependency(ruledepname);
    if (auto t = dep->getTarget().as<PredefinedProgram *>())
        return t->getRule1(rulename);
    else
        SW_UNIMPLEMENTED;
}

IRulePtr RuleSystem2::getRuleFromDependency(const String &rulename) const
{
    return getRuleFromDependency(rulename, rulename);
}

void RuleSystem2::runRules2(RuleFiles rfs, const Target &t)
{
    auto &rules = getRuleDependencies();
    for (auto &[_,rd] : rules)
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
        for (auto &[_,rd] : rules)
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

Commands RuleSystem2::getRuleCommands() const
{
    Commands c;
    for (auto &[_,rd] : getRuleDependencies())
        c.merge(rd.getRule().getCommands());
    return c;
}

} // namespace sw
