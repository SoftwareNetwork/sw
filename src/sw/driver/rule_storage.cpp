// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#include "rule_storage.h"

#include "target/base.h"

namespace sw
{

RuleStorage::RuleStorage() = default;
RuleStorage::~RuleStorage() = default;

void RuleStorage::push(const String &name, IRulePtr r)
{
    RuleData rd;
    rd.rule = std::move(r);
    if (!rules[name].empty())
    {
        auto nr = dynamic_cast<NativeRule *>(rd.rule.get());
        if (nr)
            nr->arguments.push_back(rules[name].top().arguments);
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
        bool newf = false;
        for (auto &r : rules)
        {
            auto nr = dynamic_cast<NativeRule*>(r.rule.get());
            if (!nr)
                continue;
            auto outputs = nr->addInputs(t, rfs);
            for (auto &o : outputs)
            {
                auto [_, inserted] = rfs.insert(o);
                newf |= inserted;
            }
        }
        if (!newf)
            break;
    }
}

} // namespace sw
