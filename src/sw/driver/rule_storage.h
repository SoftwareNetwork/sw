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

struct SW_DRIVER_CPP_API RuleData
{
    String rule_name;
    DependencyPtr dep;
    String target_rule_name;

    RuleData(const String &name);
    RuleData(const String &name, const TargetSettings &);
    RuleData(const String &name, const UnresolvedPackage &from_dep);
    RuleData(const String &name, const DependencyPtr &from_dep);
    RuleData(const String &name, const DependencyPtr &from_dep, const String &from_name);
    RuleData(const String &name, std::unique_ptr<IRule> r);
    RuleData(const RuleData &);
    RuleData &operator=(const RuleData &);

    IRule *getRule() const;

    auto &getArguments() { return arguments; }
    const auto &getArguments() const { return arguments; }

private:
    // shared ptr for simple copying
    // make unique_ptr later
    mutable std::shared_ptr<IRule> ptr;
    primitives::command::Arguments arguments;
};

struct SW_DRIVER_CPP_API RuleSystem
{
    void addRuleDependency(const String &rulename, const DependencyPtr &from_dep, const String &from_name);
    void addRuleDependency(const String &rulename, const DependencyPtr &from_dep);
protected:
    void addRuleDependency(const RuleData &, bool overwrite = false);

public:
    // targets may add local rules themselves
    void addRule(const String &rulename, std::unique_ptr<IRule> r);

    // targets may get "rule holder" for use in custom commands
    // dependency also may not be available at that time, so we give holder instead
    RuleData &getRule(const String &n);

protected:
    void runRules(const RuleFiles &, const Target &);
    Commands getRuleCommands() const;
    std::vector<DependencyPtr> getRuleDependencies() const;

private:
    std::map<String, RuleData> rule_dependencies;
    RuleFiles rfs;

    IRulePtr getRuleFromDependency(const String &ruledepname, const String &rulename) const;
    IRulePtr getRuleFromDependency(const String &rulename) const;
    DependencyPtr getRuleDependency(const String &rulename) const;
};

} // namespace sw
