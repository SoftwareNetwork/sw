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

RuleData::RuleData(const String &name, const DependencyPtr &from_dep, const String &from_name)
{
    rule_name = name;
    dep = from_dep;
    target_rule_name = from_name;
}

RuleData::RuleData(const String &name, const DependencyPtr &from_dep)
    : RuleData(name, from_dep, name)
{
}

RuleData::RuleData(const String &name, const UnresolvedPackageName &from_dep)
    : RuleData(name, std::make_shared<Dependency>(from_dep))
{
}

RuleData::RuleData(const String &name, const PackageSettings &ts)
    : RuleData(name, ts["rule"][name]["package"].getValue())
{
}

RuleData::RuleData(const String &name, std::unique_ptr<IRule> r)
    : rule_name(name), ptr(std::move(r))
{
}

RuleData::RuleData(const String &name)
    : rule_name(name)
{
}

RuleData::RuleData(const RuleData &rhs)
{
    operator=(rhs);
}

RuleData &RuleData::operator=(const RuleData &rhs)
{
    rule_name = rhs.rule_name;
    dep = rhs.dep;
    target_rule_name = rhs.target_rule_name;
    ptr = rhs.ptr;
    return *this;
}

IRule *RuleData::getRule() const
{
    if (ptr)
        return ptr.get();
    if (!dep)
        return {};
    auto t = dep->getTarget().as<PredefinedProgram *>();
    if (!t)
        SW_UNIMPLEMENTED;
    ptr = t->getRule1(target_rule_name);
    return ptr.get();
}

void RuleSystem::addRuleDependency(const RuleData &d, bool overwrite)
{
    auto [i,inserted] = rule_dependencies.emplace(d.rule_name, d);
    if (inserted)
        return;
    if (!overwrite)
        LOG_TRACE(logger, "Overridding rule '" + d.rule_name);
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

void RuleSystem::addRule(const String &rulename, std::unique_ptr<IRule> r)
{
    rule_dependencies.insert_or_assign(rulename, RuleData{rulename, std::move(r)});
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
    if (!dep)
        throw SW_RUNTIME_ERROR("No rule dependency for rule: " + ruledepname);
    if (auto t = dep->getTarget().as<PredefinedProgram *>())
        return t->getRule1(rulename);
    else
        SW_UNIMPLEMENTED;
}

IRulePtr RuleSystem::getRuleFromDependency(const String &rulename) const
{
    return getRuleFromDependency(rulename, rulename);
}

std::vector<DependencyPtr> RuleSystem::getRuleDependencies() const
{
    std::vector<DependencyPtr> r;
    for (auto &[_, rd] : rule_dependencies)
    {
        if (rd.dep)
            r.push_back(rd.dep);
    }
    return r;
}

RuleData &RuleSystem::getRule(const String &n)
{
    auto [i,_] = rule_dependencies.emplace(n, n);
    return i->second;
}

void RuleSystem::runRules(const RuleFiles &inrfs, const Target &t)
{
    rfs = inrfs;
    for (auto &[_,rd] : rule_dependencies)
    {
        auto r = rd.getRule();
        if (!r)
            continue;
        r->setup(t);
        auto nr = r->as<NativeRule *>();
        if (!nr)
            continue;
        nr->arguments.push_back(rd.getArguments());
    }
    while (1)
    {
        auto sz = rfs.size();
        for (auto &[_,rd] : rule_dependencies)
        {
            auto r = rd.getRule();
            if (!r)
                continue;
            if (auto nr = r->as<NativeRule *>())
                nr->addInputs(t, rfs);
        }
        if (sz == rfs.size())
            break;
    }
}

Commands RuleSystem::getRuleCommands() const
{
    return rfs.getCommands();
}

} // namespace sw
