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

#include "checks.h"

#include "context.h"

#include <boost/algorithm/string.hpp>

#include <memory>

const std::map<int, Check::Information> check_information{
    { Check::Function,
    { Check::Function, "function", "functions" } },

    { Check::Include,
    { Check::Include, "include", "includes" } },

    { Check::Type,
    { Check::Type, "type", "types" } },

    { Check::Library,
    { Check::Library, "library", "libraries" } },

    { Check::Symbol,
    { Check::Symbol, "symbol", "symbols" } },

    { Check::CSourceCompiles,
    { Check::CSourceCompiles, "c_source_compiles", "c_source_compiles" } },

    { Check::CSourceRuns,
    { Check::CSourceRuns, "c_source_runs", "c_source_runs" } },

    { Check::CXXSourceCompiles,
    { Check::CXXSourceCompiles, "cxx_source_compiles", "cxx_source_compiles" } },

    { Check::CXXSourceRuns,
    { Check::CXXSourceRuns, "cxx_source_runs", "cxx_source_runs" } },

    { Check::Custom,
    { Check::Custom, "custom", "custom" } },
};

auto getCheckInformation(int type)
{
    auto i = check_information.find(type);
    if (i == check_information.end())
        return Check::Information();
    return i->second;
}

class CheckFunction : public Check
{
public:
    CheckFunction(const String &s)
        : Check(getCheckInformation(Function))
    {
        data = s;
        variable = "HAVE_" + boost::algorithm::to_upper_copy(data);
    }

    virtual ~CheckFunction() {}
};

class CheckInclude : public Check
{
public:
    CheckInclude(const String &s)
        : Check(getCheckInformation(Include))
    {
        data = s;
        auto v_def = "HAVE_" + boost::algorithm::to_upper_copy(data);
        for (auto &c : v_def)
        {
            if (!isalnum(c))
                c = '_';
        }
        variable = v_def;
    }

    virtual ~CheckInclude() {}
};

class CheckType : public Check
{
public:
    CheckType(const String &s, const String &prefix = "HAVE_")
        : Check(getCheckInformation(Type))
    {
        data = s;
        String v_def = prefix;
        v_def += boost::algorithm::to_upper_copy(s);
        for (auto &c : v_def)
        {
            if (c == '*')
                c = 'P';
            else if (!isalnum(c))
                c = '_';
        }
        variable = v_def;
    }

    virtual ~CheckType() {}
};

class CheckLibrary : public Check
{
public:
    CheckLibrary(const String &s)
        : Check(getCheckInformation(Library))
    {
        data = s;
        auto v_def = "HAVE_LIB" + boost::algorithm::to_upper_copy(data);
        for (auto &c : v_def)
        {
            if (!isalnum(c))
                c = '_';
        }
        variable = v_def;
    }

    virtual ~CheckLibrary() {}
};

class CheckSymbol : public Check
{
public:
    CheckSymbol(const String &s, const std::set<String> &headers)
        : Check(getCheckInformation(Symbol)),
          headers(headers)
    {
        data = s;
        variable = "HAVE_" + boost::algorithm::to_upper_copy(data);
    }

    virtual ~CheckSymbol() {}

private:
    std::set<String> headers;
};

class CheckCSourceCompiles : public Check
{
public:
    CheckCSourceCompiles(const String &var, const String &d)
        : Check(getCheckInformation(CSourceCompiles))
    {
        variable = var;
        data = d;
    }

    virtual ~CheckCSourceCompiles() {}
};

class CheckCSourceRuns : public Check
{
public:
    CheckCSourceRuns(const String &var, const String &d)
        : Check(getCheckInformation(CSourceRuns))
    {
        variable = var;
        data = d;
    }

    virtual ~CheckCSourceRuns() {}
};

class CheckCXXSourceCompiles : public Check
{
public:
    CheckCXXSourceCompiles(const String &var, const String &d)
        : Check(getCheckInformation(CXXSourceCompiles))
    {
        variable = var;
        data = d;
    }

    virtual ~CheckCXXSourceCompiles() {}
};

class CheckCXXSourceRuns : public Check
{
public:
    CheckCXXSourceRuns(const String &var, const String &d)
        : Check(getCheckInformation(CXXSourceRuns))
    {
        variable = var;
        data = d;
    }

    virtual ~CheckCXXSourceRuns() {}
};

class CheckCustom : public Check
{
public:
    CheckCustom(const String &var, const String &d)
        : Check(getCheckInformation(Custom))
    {
        variable = var;
        data = d;
    }

    virtual ~CheckCustom() {}
};

Check::Check(const Information &i)
    : information(i)
{
}

template <class T, class ... Args>
void Checks::addCheck(Args && ... args)
{
    auto i = std::make_shared<T>(std::forward<Args>(args)...);
    auto p = i.get();
    checks[p->getVariable()] = std::move(i);
}

void Checks::load(const yaml &root)
{
#define ADD_SET_CHECKS(t, s)                      \
    do                                            \
    {                                             \
        auto seq = get_sequence<String>(root, s); \
        for (auto &v : seq)                       \
            addCheck<t>(v);                       \
    } while (0)

    ADD_SET_CHECKS(CheckFunction, "check_function_exists");
    ADD_SET_CHECKS(CheckInclude, "check_include_exists");
    ADD_SET_CHECKS(CheckType, "check_type_size");
    ADD_SET_CHECKS(CheckLibrary, "check_library_exists");

    // add some common types
    addCheck<CheckType>("size_t");
    addCheck<CheckType>("void *");

    // symbols
    get_map_and_iterate(root, "check_symbol_exists", [this](const auto &root)
    {
        auto f = root.first.template as<String>();
        auto s = root.second.template as<String>();
        if (root.second.IsSequence() || root.second.IsScalar())
            addCheck<CheckSymbol>(f, get_sequence_set<String>(root.second));
        else
            throw std::runtime_error("Symbol headers should be a scalar or a set");
    });

#define LOAD_MAP(t, s)                                   \
    get_map_and_iterate(root, s, [this](const auto &v) { \
        auto fi = v.first.template as<String>();         \
        auto se = v.second.template as<String>();        \
        addCheck<t>(fi, se);                             \
    })

    LOAD_MAP(CheckCSourceCompiles, "check_c_source_compiles");
    LOAD_MAP(CheckCSourceRuns, "check_c_source_runs");
    LOAD_MAP(CheckCXXSourceCompiles, "check_cxx_source_compiles");
    LOAD_MAP(CheckCXXSourceRuns, "check_cxx_source_runs");

    LOAD_MAP(CheckCustom, "checks");
}

bool Checks::empty() const
{
    return checks.empty();
}

Checks &Checks::operator+=(const Checks &rhs)
{
    checks.insert(rhs.checks.begin(), rhs.checks.end());
    return *this;
}

void Checks::write_parallel_checks(Context &ctx) const
{
    for (int t = 0; t < Check::Max; t++)
    {
        ctx.addLine("set(vars_" + getCheckInformation(t).plural + ")");
        ctx.addLine("file(WRITE ${tmp_dir}/" + getCheckInformation(t).plural + ".txt \"\")");
        ctx.addLine();
    }

    for (auto &c : checks)
    {
        auto t = c.second->getInformation().type;
        switch (t)
        {
        case Check::Function:
        case Check::Include:
        case Check::Type:
        case Check::Library:
            ctx.addLine("if (NOT DEFINED " + c.second->getVariable() + ")");
            ctx.addLine("    list(APPEND vars_" + getCheckInformation(t).plural + " \"" + c.second->getData() + "\")");
            ctx.addLine("endif()");
            break;
        // add more parallel checks here
        }
    }

    for (int t = 0; t < Check::Max; t++)
    {
        ctx.addLine();
        ctx.addLine("list(APPEND vars_all ${vars_" + getCheckInformation(t).plural + "})");
        ctx.addLine("foreach(v ${vars_" + getCheckInformation(t).plural + "})");
        ctx.addLine("    file(APPEND ${tmp_dir}/" + getCheckInformation(t).plural + ".txt \"${v}\\n\")");
        ctx.addLine("endforeach()");
        ctx.addLine();
    }
}
