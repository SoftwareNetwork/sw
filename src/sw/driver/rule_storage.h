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

struct SW_DRIVER_CPP_API RuleSystem
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
    RuleProperties &getRule(const String &n) { return rule_properties[n]; }

protected:
    void runRules(const RuleFiles &, const Target &);
    Commands getRuleCommands() const;
    std::vector<DependencyPtr> getRuleDependencies() const;

private:
    std::map<String, RuleDescription> rule_dependencies;
    std::map<String, RuleProperties> rule_properties;
    RuleFiles rfs;

    DependencyPtr getRuleDependency(const String &rulename) const;
    IRulePtr getRuleFromDependency(const String &ruledepname, const String &rulename) const;
    IRulePtr getRuleFromDependency(const String &rulename) const;
};

} // namespace sw
