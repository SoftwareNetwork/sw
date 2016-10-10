/*
 * Copyright (c) 2016, Egor Pugin
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     1. Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *     3. Neither the name of the copyright holder nor the names of
 *        its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "common.h"
#include "yaml.h"

class Context;

class Check
{
public:
    enum
    {
        Function,
        Include,
        Type,
        Library,
        Symbol,
        CSourceCompiles,
        CSourceRuns,
        CXXSourceCompiles,
        CXXSourceRuns,
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

    virtual void writeCheck(Context &ctx) const {}
    virtual void save(yaml &root) const {}

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
    void load(const path &dir);
    void save(yaml &root) const;

    String write_checks() const;
    void write_checks(Context &ctx) const;
    void write_parallel_checks(Context &ctx) const;
    void write_parallel_checks_for_workers(Context &ctx) const;
    void read_parallel_checks_for_workers(const path &dir);
    void write_definitions(Context &ctx) const;

    std::vector<Checks> scatter(int N) const;
    void print_values() const;
    void print_values(Context &ctx) const;

    Checks &operator+=(const Checks &rhs);

private:
    template <class T, class ... Args>
    T *addCheck(Args && ... args);
};
