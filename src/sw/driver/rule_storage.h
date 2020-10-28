// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <sw/builder/command.h>

#include <memory>
#include <stack>

namespace sw
{

struct IRule;
using RulePtr = std::unique_ptr<IRule>;

struct RuleData1
{
    RulePtr rule;
    primitives::command::Arguments arguments;
};

struct RuleStorage
{
    RuleStorage();
    ~RuleStorage();

    // or set rule?
    void push(const String &name, RulePtr);
    RulePtr pop(const String &name);

    bool contains(const String &name) const;
    IRule *getRule(const String &n) const;

    void clear(); // everything
    void clear(const String &name); // only name

    Commands getCommands() const;

private:
    std::map<String, std::stack<RulePtr>> rules;
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

protected:
    Commands getRuleCommands() const;

private:
    RuleStorage rules;
};

} // namespace sw
