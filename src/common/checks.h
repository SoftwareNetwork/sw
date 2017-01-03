/*
 * Copyright (C) 2016-2017, Egor Pugin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "cppan_string.h"
#include "filesystem.h"
#include "yaml.h"

class Context;
struct Package;

class Check
{
public:
    enum
    {
        Function,
        Include,
        Type,
        Alignment,
        Library,
        LibraryFunction,
        Symbol,
        CSourceCompiles,
        CSourceRuns,
        CXXSourceCompiles,
        CXXSourceRuns,
        Decl, // decl goes almost at the end!!! (sort order)
        Custom,

        Max,
    };

    struct Information
    {
        int type;

        String cppan_key;
        String function;

        // strings for printing/naming files
        String singular;
        String plural;
    };

public:
    // maybe variant: int, double, string?
    using Value = int;

public:
    Check(const Information &i);
    virtual ~Check() {}

    const Information &getInformation() const { return information; }
    String getVariable() const { return variable; }
    String getData() const { return data; }
    String getDataEscaped() const;
    Value getValue() const { return value; }
    String getMessage() const { return message; }

    virtual void writeCheck(Context &/*ctx*/) const {}
    virtual void save(yaml &/*root*/) const {}

    void setValue(const Value &v) { value = v; }

protected:
    // self-information
    Information information;

    // e.g. HAVE_STDINT_H
    String variable;

    // symbol name (function, include, c/cxx source etc.)
    // or source code
    // or whatever
    String data;

    // (cmake) value
    Value value = 0;

    // message for printing
    String message;

public:
    // default check won't be printed
    bool default_ = false;

private:
    template <class T>
    friend struct less_check;
};

using CheckPtr = std::shared_ptr<Check>;

template <class T>
struct less_check
{
    bool operator()(const T &p1, const T &p2) const
    {
        if (p1 && p2)
            return std::tie(p1->information.type, p1->variable) < std::tie(p2->information.type, p2->variable);
        return p1 < p2;
    }
};

struct Checks
{
    std::set<CheckPtr, less_check<CheckPtr>> checks;

    bool empty() const;

    void load(const yaml &root);
    void load(const path &fn);
    void save(yaml &root) const;
    String save() const;

    void write_checks(Context &ctx) const;
    void write_parallel_checks_for_workers(Context &ctx) const;
    void read_parallel_checks_for_workers(const path &dir);
    void write_definitions(Context &ctx, const Package &d) const;

    void remove_known_vars(const std::set<String> &known_vars);
    std::vector<Checks> scatter(int N) const;
    void print_values() const;
    void print_values(Context &ctx) const;

    Checks &operator+=(const Checks &rhs);

    template <class T, class ... Args>
    T *addCheck(Args && ... args);
};

Check::Information getCheckInformation(int type);

struct ParallelCheckOptions
{
    path dir;
    path vars_file;
    path checks_file;
    String generator;
    String toolset;
    String toolchain;
};
