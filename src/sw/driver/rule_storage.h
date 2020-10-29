// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "rule.h"

#include <sw/builder/command.h>

#include <memory>
#include <stack>

namespace sw
{

struct RuleData1
{
    IRulePtr rule;
    primitives::command::Arguments arguments;
};

struct RuleStorage
{
    using Rules = std::map<String, std::stack<IRulePtr>>;

    RuleStorage();
    ~RuleStorage();

    // or set rule?
    void push(const String &name, IRulePtr);
    IRulePtr pop(const String &name);

    bool contains(const String &name) const;
    IRule *getRule(const String &n) const;

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
        auto &operator*() { return *i->second.top(); }
    };
    iter begin() { return iter{ rules.begin() }; }
    iter end() { return iter{ rules.end() }; }

private:
    Rules rules;
};

struct RuleSystem
{
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

    IRule *getRule(const String &n) const { return rules.getRule(n); }

    template <class T>
    T getRule(const String &n) const
    {
        auto r = getRule(n);
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

} // namespace sw
