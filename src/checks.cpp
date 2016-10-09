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
#include "printers/printer.h"

#include <boost/algorithm/string.hpp>

#include <memory>

#include "logger.h"
DECLARE_STATIC_LOGGER(logger, "checks");

const std::map<int, Check::Information> check_information{
    { Check::Function,
    { Check::Function, "check_function_exists", "function", "functions" } },

    { Check::Include,
    { Check::Include, "check_include_files", "include", "includes" } },

    { Check::Type,
    { Check::Type, "check_type_size", "type", "types" } },

    { Check::Library,
    { Check::Library, "find_library", "library", "libraries" } },

    { Check::Symbol,
    { Check::Symbol, "check_cxx_symbol_exists", "symbol", "symbols" } },

    { Check::CSourceCompiles,
    { Check::CSourceCompiles, "check_c_source_compiles", "c_source_compiles", "c_source_compiles" } },

    { Check::CSourceRuns,
    { Check::CSourceRuns, "check_c_source_runs", "c_source_runs", "c_source_runs" } },

    { Check::CXXSourceCompiles,
    { Check::CXXSourceCompiles, "check_cxx_source_compiles", "cxx_source_compiles", "cxx_source_compiles" } },

    { Check::CXXSourceRuns,
    { Check::CXXSourceRuns, "check_cxx_source_runs", "cxx_source_runs", "cxx_source_runs" } },

    { Check::Custom,
    { Check::Custom, "", "custom", "custom" } },
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
    CheckSymbol() : Check(getCheckInformation(Symbol)) {}

    CheckSymbol(const String &s, const std::set<String> &headers)
        : Check(getCheckInformation(Symbol)),
          headers(headers)
    {
        data = s;
        variable = "HAVE_" + boost::algorithm::to_upper_copy(data);
    }

    virtual ~CheckSymbol() {}

    void writeCheck(Context &ctx) const override
    {
        ctx << information.function + "(\"" + getData() + "\" \"";
        for (auto &h : headers)
            ctx << h << ";";
        ctx << "\" " << getVariable() << ")" << Context::eol;
    }

private:
    std::set<String> headers;
};

struct CheckSource : public Check
{
    CheckSource(const Check::Information &i)
        : Check(i)
    {
    }

    virtual ~CheckSource() {}

    bool invert = false;
};

class CheckCSourceCompiles : public CheckSource
{
public:
    CheckCSourceCompiles(const String &var, const String &d)
        : CheckSource(getCheckInformation(CSourceCompiles))
    {
        variable = var;
        data = d;
    }

    virtual ~CheckCSourceCompiles() {}
};

class CheckCSourceRuns : public CheckSource
{
public:
    CheckCSourceRuns(const String &var, const String &d)
        : CheckSource(getCheckInformation(CSourceRuns))
    {
        variable = var;
        data = d;
    }

    virtual ~CheckCSourceRuns() {}
};

class CheckCXXSourceCompiles : public CheckSource
{
public:
    CheckCXXSourceCompiles(const String &var, const String &d)
        : CheckSource(getCheckInformation(CXXSourceCompiles))
    {
        variable = var;
        data = d;
    }

    virtual ~CheckCXXSourceCompiles() {}
};

class CheckCXXSourceRuns : public CheckSource
{
public:
    CheckCXXSourceRuns(const String &var, const String &d)
        : CheckSource(getCheckInformation(CXXSourceRuns))
    {
        variable = var;
        data = d;
    }

    virtual ~CheckCXXSourceRuns() {}
};

class CheckCustom : public CheckSource
{
public:
    CheckCustom(const String &var, const String &d)
        : CheckSource(getCheckInformation(Custom))
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

String Check::getDataEscaped() const
{
    auto d = getData();
    boost::replace_all(d, "\\", "\\\\\\\\");
    boost::replace_all(d, "\"", "\\\"");
    return d;
}

template <class T, class ... Args>
T *Checks::addCheck(Args && ... args)
{
    auto i = std::make_shared<T>(std::forward<Args>(args)...);
    auto p = i.get();
    checks.emplace(std::move(i));
    return p;
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
        if (root.second.IsSequence() || root.second.IsScalar())
            this->addCheck<CheckSymbol>(f, get_sequence_set<String>(root.second));
        else
            throw std::runtime_error("Symbol headers should be a scalar or a set");
    });

#define LOAD_MAP(t, s)                                                  \
    get_map_and_iterate(root, s, [this](const auto &v) {                \
        auto fi = v.first.template as<String>();                        \
        if (v.second.IsScalar())                                        \
        {                                                               \
            auto se = v.second.template as<String>();                   \
            this->addCheck<t>(fi, se);                                  \
        }                                                               \
        else if (v.second.IsMap())                                      \
        {                                                               \
            auto se = v.second["text"].template as<String>();           \
            auto p = this->addCheck<t>(fi, se);                         \
            if (v.second["invert"].IsDefined())                         \
                p->invert = v.second["invert"].template as<bool>();     \
        }                                                               \
        else                                                            \
        {                                                               \
            throw std::runtime_error(s " should be a scalar or a map"); \
        }                                                               \
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

void Checks::write_checks(Context &ctx) const
{
    auto invert = [&ctx](auto &c)
    {
        ctx.addLine("if (" + c->getVariable() + ")");
        ctx.addLine("set(" + c->getVariable() + " 0)");
        ctx.addLine("else()");
        ctx.addLine("set(" + c->getVariable() + " 1)");
        ctx.addLine("endif()");
    };

    for (auto &c : checks)
    {
        auto &i = c->getInformation();
        auto t = i.type;

        ctx.addLine("if (NOT DEFINED " + c->getVariable() + ")");
        ctx.increaseIndent();

        switch (t)
        {
        case Check::Function:
        case Check::Include:
        case Check::Type:
            ctx.addLine(i.function + "(\"" + c->getData() + "\" " + c->getVariable() + ")");
            if (t == Check::Type)
            {
                CheckType ct (c->getData(), "SIZEOF_");
                CheckType ct_(c->getData(), "SIZE_OF_");

                ctx.addLine("if (" + c->getVariable() + ")");
                ctx.increaseIndent();
                ctx.addLine("set(" + ct_.getVariable() + " ${" + c->getVariable() + "} CACHE STRING \"\")");
                ctx.addLine("set(" + ct.getVariable() + " ${" + c->getVariable() + "} CACHE STRING \"\")");
                ctx.decreaseIndent();
                ctx.addLine("endif()");
            }
            break;
        case Check::Library:
            ctx.addLine("find_library(" + c->getVariable() + " " + c->getData() + ")");
            ctx.addLine("if (\"${" + c->getVariable() + "}\" STREQUAL \"" + c->getVariable() + "-NOTFOUND\")");
            ctx.addLine("    set(" + c->getVariable() + " 0)");
            ctx.addLine("else()");
            ctx.addLine("    set(" + c->getVariable() + " 1)");
            ctx.addLine("endif()");
            break;
        case Check::Symbol:
            c->writeCheck(ctx);
            break;
        case Check::CSourceCompiles:
        case Check::CSourceRuns:
        case Check::CXXSourceCompiles:
        case Check::CXXSourceRuns:
            ctx.addLine(i.function + "(\"" + c->getDataEscaped() + "\" " + c->getVariable() + ")");
            {
                auto p = (CheckSource *)c.get();
                if (p->invert)
                    invert(c);
            }
            break;
        case Check::Custom:
            ctx.addLine(c->getData());
            {
                auto p = (CheckSource *)c.get();
                if (p->invert)
                    invert(c);
            }
            break;
        }

        ctx.addLine("add_variable(" + c->getVariable() + ")");
        ctx.decreaseIndent();
        ctx.addLine("endif()");
        ctx.addLine();
    }
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
        auto t = c->getInformation().type;
        switch (t)
        {
        case Check::Function:
        case Check::Include:
        case Check::Type:
        case Check::Library:
            ctx.addLine("if (NOT DEFINED " + c->getVariable() + ")");
            ctx.addLine("    list(APPEND vars_" + getCheckInformation(t).plural + " \"" + c->getData() + "\")");
            ctx.addLine("endif()");
            break;
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

void Checks::write_parallel_checks_for_workers(Context &ctx) const
{
    for (auto &c : checks)
    {
        auto &i = c->getInformation();
        auto t = i.type;
        switch (t)
        {
        case Check::Function:
        case Check::Include:
        case Check::Type:
            ctx.addLine(i.function + "(\"" + c->getData() + "\" " + c->getVariable() + ")");
            ctx.addLine("if (NOT " + c->getVariable() + ")");
            ctx.addLine("    set(" + c->getVariable() + " 0)");
            ctx.addLine("endif()");
            ctx.addLine("file(WRITE " + c->getVariable() + " \"${" + c->getVariable() + "}\")");
            ctx.addLine();
            break;
        case Check::Library:
            ctx.addLine("find_library(" + c->getVariable() + " " + c->getData() + ")");
            ctx.addLine("if (\"${" + c->getVariable() + "}\" STREQUAL \"" + c->getVariable() + "-NOTFOUND\")");
            ctx.addLine("    set(" + c->getVariable() + " 0)");
            ctx.addLine("else()");
            ctx.addLine("    set(" + c->getVariable() + " 1)");
            ctx.addLine("endif()");
            ctx.addLine("file(WRITE " + c->getVariable() + " \"${" + c->getVariable() + "}\")");
            break;
        }
    }
}

void Checks::read_parallel_checks_for_workers(const path &dir)
{
    for (auto &c : checks)
    {
        auto s = read_file(dir / c->getVariable());
        c->setValue(std::stoi(s));
    }
}

void Checks::write_definitions(Context &ctx) const
{
    auto print_def = [&ctx](const String &value, auto &&s)
    {
        ctx << "INTERFACE " << s << "=" << value << Context::eol;
        return 0;
    };

    auto add_if_definition = [&ctx, &print_def](const String &s, const String &value, auto && ... defs)
    {
        ctx.addLine("if (" + s + ")");
        ctx.increaseIndent();
        ctx << "target_compile_definitions(" << cppan_helpers_target << Context::eol;
        ctx.increaseIndent();
        print_def(value, s);
        using expand_type = int[];
        expand_type{ 0, print_def(value, std::forward<decltype(defs)>(defs))... };
        ctx.decreaseIndent();
        ctx.addLine(")");
        ctx.decreaseIndent();
        ctx.addLine("endif()");
        ctx.addLine();
    };

    // aliases
    add_if_definition("WORDS_BIGENDIAN", "1", "BIGENDIAN", "BIG_ENDIAN", "HOST_BIG_ENDIAN");

    for (auto &c : checks)
    {
        auto &i = c->getInformation();
        auto t = i.type;

        add_if_definition(c->getVariable(), "1");

        if (t == Check::Type)
        {
            CheckType ct(c->getData(), "SIZEOF_");
            CheckType ct_(c->getData(), "SIZE_OF_");

            add_if_definition(ct.getVariable(), "${" + ct.getVariable() + "}");
            add_if_definition(ct_.getVariable(), "${" + ct_.getVariable() + "}");
        }
    }
}

void Checks::load(const path &dir)
{
    auto get_lines = [](const auto &s)
    {
        std::vector<String> v, lines;
        boost::split(v, s, boost::is_any_of("\r\n"));
        for (auto &line : v)
        {
            boost::trim(line);
            if (line.empty())
                continue;
            lines.push_back(line);
        }
        return lines;
    };

    for (int t = 0; t < Check::Max; t++)
    {
        auto s = read_file(dir / (getCheckInformation(t).plural + ".txt"));

#define CASE_LINE(x)                 \
    case Check::x:                   \
        for (auto &v : get_lines(s)) \
            addCheck<Check##x>(v);   \
        break

        switch (t)
        {
            CASE_LINE(Function);
            CASE_LINE(Include);
            CASE_LINE(Type);
            CASE_LINE(Library);
        }

#undef CASE_LINE
    }
}

std::vector<Checks> Checks::scatter(int N) const
{
    std::vector<Checks> workers(N);
    int i = 0;
    for (auto &c : checks)
    {
        auto &inf = c->getInformation();
        auto t = inf.type;
        switch (t)
        {
        case Check::Function:
        case Check::Include:
        case Check::Type:
        case Check::Library:
            workers[i++ % N].checks.insert(c);
            break;
        }
    }
    return workers;
}

void Checks::print_values() const
{
    for (auto &c : checks)
    {
        auto &i = c->getInformation();
        auto t = i.type;
        switch (t)
        {
        case Check::Function:
        case Check::Include:
        case Check::Type:
        case Check::Library:
            if (c->getValue())
                LOG_INFO(logger, "-- " << i.singular << " " + c->getData() + " - found (" + std::to_string(c->getValue()) + ")");
            else
                LOG_INFO(logger, "-- " << i.singular << " " + c->getData() + " - not found");
            break;
        }
    }
}

void Checks::print_values(Context &ctx) const
{
    for (auto &c : checks)
    {
        ctx.addLine("STRING;" + c->getVariable() + ";" + std::to_string(c->getValue()));
    }
}
