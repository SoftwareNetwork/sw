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
    Value getValue() const { return value; }
    String getMessage() const { return message; }

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
};

struct Checks
{
    // first: variable (HAVE_SOMETHING), second: Check
    std::map<String, std::shared_ptr<Check>> checks;

    bool empty() const;
    void load(const yaml &root);

    void write_parallel_checks(Context &ctx) const;

    Checks &operator+=(const Checks &rhs);

private:
    template <class T, class ... Args>
    void addCheck(Args && ... args);
};
