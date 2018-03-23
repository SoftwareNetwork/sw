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

#include "context.h"
#include "cppan_string.h"
#include "filesystem.h"
#include "yaml.h"

class CMakeContext;
struct Package;

struct CheckParameters
{
    Strings headers;
    StringSet definitions;
    StringSet include_directories;
    StringSet libraries;
    StringSet flags;

    // TODO: pass all found includes to this test
    // it is possible only in sequential mode
    bool all_includes = false;

    void writeHeadersBefore(CMakeContext &ctx) const;
    void writeHeadersAfter(CMakeContext &ctx) const;
    void writeBefore(CMakeContext &ctx) const;
    void writeAfter(CMakeContext &ctx) const;
    void load(const yaml &n);
    void save(yaml &n) const;
    bool empty() const;
    String getHash() const;
    bool operator<(const CheckParameters &p) const;
};

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
        StructMember,
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
    Check(const Information &i, const CheckParameters &parameters = CheckParameters());
    virtual ~Check() {}

    const Information &getInformation() const { return information; }
    String getVariable() const { return variable; }
    String getData() const { return data; }
    String getDataEscaped() const;
    Value getValue() const { return value; }
    String getMessage() const { return message; }

    virtual void writeCheck(CMakeContext &/*ctx*/) const {}
    virtual void save(yaml &/*root*/) const {}

    void setValue(const Value &v) { value = v; }

    bool get_cpp() const { return cpp; }
    virtual void set_cpp(bool) {}

    String getFileName() const;

    virtual String printStatus() const
    {
        if (getValue())
            return "-- " + information.singular + " " + getData() + " - found (" + std::to_string(getValue()) + ")";
        else
            return "-- " + information.singular + " " + getData() + " - not found";
    }

    virtual bool isOk() const
    {
        return !!getValue();
    }

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

    bool cpp = false;

public:
    // default check won't be printed
    bool default_ = false;

    // parameters
    CheckParameters parameters;

public:
    static String make_include_var(const String &i);
    static String make_type_var(const String &t, const String &prefix = "HAVE_");
    static String make_struct_member_var(const String &m, const String &s);

private:
    template <class T>
    friend struct CheckPtrLess;
};

using CheckPtr = std::shared_ptr<Check>;

template <class T>
struct CheckPtrLess
{
    bool operator()(const T &p1, const T &p2) const
    {
        if (p1 && p2)
            return
            std::tie(p1->information.type, p1->variable, p1->parameters) <
            std::tie(p2->information.type, p2->variable, p2->parameters);
        return p1 < p2;
    }
};

using ChecksSet = std::set<CheckPtr, CheckPtrLess<CheckPtr>>;

struct Checks
{
    ChecksSet checks;
    bool valid = true;

    bool empty() const;

    void load(const yaml &root);
    void load(const path &fn);
    void save(yaml &root) const;
    String save() const;

    void write_checks(CMakeContext &ctx, const StringSet &prefixes = StringSet()) const;
    void write_definitions(CMakeContext &ctx, const Package &d, const StringSet &prefixes = StringSet()) const;

    void write_parallel_checks_for_workers(CMakeContext &ctx) const;
    void read_parallel_checks_for_workers(const path &dir);

    void remove_known_vars(const std::set<String> &known_vars);
    std::vector<Checks> scatter(int N) const;
    void print_values() const;
    void print_values(CMakeContext &ctx) const;

    Checks &operator+=(const Checks &rhs);

    template <class T, class ... Args>
    T *addCheck(Args && ... args);
};

Check::Information getCheckInformation(int type);

struct ParallelCheckOptions
{
    path cmake_binary;
    path dir;
    path vars_file;
    path checks_file;
    String generator;
    String system_version;
    String toolset;
    String toolchain;
};
