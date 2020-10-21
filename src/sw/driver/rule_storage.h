// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <sw/builder/command.h>

#include <memory>
#include <stack>

namespace sw
{

struct IRule;

struct RuleStorage
{
    RuleStorage();
    ~RuleStorage();

    // or set rule?
    void push(const String &name, std::unique_ptr<IRule>);
    std::unique_ptr<IRule> pop(const String &name);

    bool contains(const String &name) const;

    void clear(); // everything
    void clear(const String &name); // only name

    Commands getCommands() const;

private:
    std::map<String, std::stack<std::unique_ptr<IRule>>> rules;
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

    Commands getRuleCommands() const;

private:
    RuleStorage rules;
};

} // namespace sw
