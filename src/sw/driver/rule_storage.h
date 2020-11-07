// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "dependency.h"
#include "rule.h"

#include <sw/builder/command.h>

#include <memory>
#include <stack>

namespace sw
{

struct SW_DRIVER_CPP_API RuleData1
{
    IRulePtr rule;

    RuleData1() = default;
    RuleData1(RuleData1 &&) = default;
    RuleData1(const RuleData1 &) = delete;
    RuleData1 &operator=(const RuleData1 &) = delete;

    auto &getArguments() { return arguments; }
    const auto &getArguments() const { return arguments; }

private:
    primitives::command::Arguments arguments;
};

struct SW_DRIVER_CPP_API RuleStorage
{
    using RuleData = RuleData1;
    using Rules = std::map<String, std::stack<RuleData>>;

    RuleStorage() = default;
    RuleStorage(const RuleStorage &) = delete;
    RuleStorage &operator=(const RuleStorage &) = delete;

    // or set rule?
    void push(const String &name, IRulePtr);
    IRulePtr pop(const String &name);

    bool contains(const String &name) const;
    RuleData &getRule(const String &n);
    const RuleData &getRule(const String &n) const;

    void clear(); // everything
    void clear(const String &name); // only name

    Commands getCommands() const;

    struct iter
    {
        using iterator = typename Rules::iterator;
        iterator i;
        iter(iterator i) : i(i) {}
        auto operator<=>(const iter &) const = default;
        iter &operator++() { ++i; return *this; }
        auto &operator*() { return i->second.top(); }
    };
    iter begin() { return iter{ rules.begin() }; }
    iter end() { return iter{ rules.end() }; }

private:
    Rules rules;
};

struct SW_DRIVER_CPP_API RuleSystem
{
    RuleSystem() = default;
    RuleSystem(const RuleSystem &) = delete;

    // return ptr?
    template <class T>
    T &addRule(const String &n, std::unique_ptr<T> r)
    {
        auto ptr = r.get();
        rules.push(n, std::move(r));
        return *ptr;
    }

    /*template <class T>
    T &overrideRule(const String &n, std::unique_ptr<T> r)
    {
        if (!rules.contains(n))
            throw SW_RUNTIME_ERROR("No previous rule: " + n);
        return addRule(n, std::move(r));
    }*/

    RuleStorage::RuleData &getRule(const String &n) { return rules.getRule(n); }
    const RuleStorage::RuleData &getRule(const String &n) const { return rules.getRule(n); }

    template <class T>
    T getRule(const String &n) const
    {
        auto &r = getRule(n);
        if (!r)
            return {};
        return r->as<T>();
    }

    void runRules(RuleFiles rfs, const Target &);

protected:
    Commands getRuleCommands() const;

private:
    RuleStorage rules;
};

struct SW_DRIVER_CPP_API RuleSystem2
{
    struct RuleDescription
    {
        String rule_name;
        DependencyPtr dep;
        String target_rule_name;

        RuleDescription(const String &name, const TargetSettings &);
        RuleDescription(const String &name, const UnresolvedPackage &from_dep);
        RuleDescription(const String &name, const DependencyPtr &from_dep);
        RuleDescription(const String &name, const DependencyPtr &from_dep, const String &from_name);

        IRule &getRule() const;

    private:
        mutable std::shared_ptr<IRule> ptr;
    };
    struct RuleProperties
    {
        auto &getArguments() { return arguments; }
        const auto &getArguments() const { return arguments; }

    private:
        primitives::command::Arguments arguments;
    };

    void addRuleDependency(const String &rulename, const DependencyPtr &from_dep, const String &from_name);
    void addRuleDependency(const String &rulename, const DependencyPtr &from_dep);
protected:
    void addRuleDependency(const RuleDescription &, bool overwrite = false);
public:

    auto &getRuleDependencies() { return rule_dependencies; }
    const auto &getRuleDependencies() const { return rule_dependencies; }

    RuleProperties &getRule(const String &n) { return rule_properties[n]; }

protected:
    void runRules2(RuleFiles rfs, const Target &t);
    Commands getRuleCommands() const;

private:
    std::map<String, RuleDescription> rule_dependencies;
    std::map<String, RuleProperties> rule_properties;

    DependencyPtr getRuleDependency(const String &rulename) const;
    IRulePtr getRuleFromDependency(const String &ruledepname, const String &rulename) const;
    IRulePtr getRuleFromDependency(const String &rulename) const;
};

} // namespace sw
