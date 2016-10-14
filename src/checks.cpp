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
#include "checks_detail.h"

#include "context.h"
#include "printers/printer.h"

#include <boost/algorithm/string.hpp>

#include <memory>

#include "logger.h"
DECLARE_STATIC_LOGGER(logger, "checks");

const std::map<int, Check::Information> check_information{
    { Check::Function,
    { Check::Function, "check_function_exists", "check_function_exists", "function", "functions" } },

    { Check::Include,
    { Check::Include, "check_include_exists", "check_include_files", "include", "includes" } },

    { Check::Type,
    { Check::Type, "check_type_size", "check_type_size", "type", "types" } },

    { Check::Library,
    { Check::Library, "check_library_exists", "find_library", "library", "libraries" } },

    { Check::Symbol,
    { Check::Symbol, "check_symbol_exists", "check_cxx_symbol_exists", "symbol", "symbols" } },

    { Check::CSourceCompiles,
    { Check::CSourceCompiles, "check_c_source_compiles", "check_c_source_compiles", "c_source_compiles", "c_source_compiles" } },

    { Check::CSourceRuns,
    { Check::CSourceRuns, "check_c_source_runs", "check_c_source_runs", "c_source_runs", "c_source_runs" } },

    { Check::CXXSourceCompiles,
    { Check::CXXSourceCompiles, "check_cxx_source_compiles", "check_cxx_source_compiles", "cxx_source_compiles", "cxx_source_compiles" } },

    { Check::CXXSourceRuns,
    { Check::CXXSourceRuns, "check_cxx_source_runs", "check_cxx_source_runs", "cxx_source_runs", "cxx_source_runs" } },

    { Check::Custom,
    { Check::Custom, "checks", "", "custom", "custom" } },
};

Check::Information getCheckInformation(int type)
{
    auto i = check_information.find(type);
    if (i == check_information.end())
        return Check::Information();
    return i->second;
}

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

void Checks::load(const yaml &root)
{
#define LOAD_SET(t)                                                                     \
    do                                                                                  \
    {                                                                                   \
        auto seq = get_sequence<String>(root, getCheckInformation(Check::t).cppan_key); \
        for (auto &v : seq)                                                             \
            addCheck<Check##t>(v);                                                      \
    } while (0)

    LOAD_SET(Function);
    LOAD_SET(Library);
    LOAD_SET(Type);

    // add some common types
    addCheck<CheckType>("size_t");
    addCheck<CheckType>("void *");

    // includes
    get_sequence_and_iterate(root, getCheckInformation(Check::Include).cppan_key, [this](const auto &v)
    {
        if (v.IsScalar())
        {
            this->addCheck<CheckInclude>(v.template as<String>());
        }
        else if (v.IsMap())
        {
            auto f = v["file"].template as<String>();
            auto var = v["variable"].template as<String>();
            this->addCheck<CheckInclude>(f, var);
        }
    });

    // symbols
    get_map_and_iterate(root, getCheckInformation(Check::Symbol).cppan_key, [this](const auto &root)
    {
        auto f = root.first.template as<String>();
        if (root.second.IsSequence() || root.second.IsScalar())
            this->addCheck<CheckSymbol>(f, get_sequence_set<String>(root.second));
        else
            throw std::runtime_error("Symbol headers should be a scalar or a set");
    });

#define LOAD_MAP(t)                                                                                             \
    get_map_and_iterate(root, getCheckInformation(Check::t).cppan_key, [this](const auto &v) {                  \
        auto fi = v.first.template as<String>();                                                                \
        if (v.second.IsScalar())                                                                                \
        {                                                                                                       \
            auto se = v.second.template as<String>();                                                           \
            this->addCheck<Check##t>(fi, se);                                                                   \
        }                                                                                                       \
        else if (v.second.IsMap())                                                                              \
        {                                                                                                       \
            auto se = v.second["text"].template as<String>();                                                   \
            auto p = this->addCheck<Check##t>(fi, se);                                                          \
            if (v.second["invert"].IsDefined())                                                                 \
                p->invert = v.second["invert"].template as<bool>();                                             \
        }                                                                                                       \
        else                                                                                                    \
        {                                                                                                       \
            throw std::runtime_error(getCheckInformation(Check::t).cppan_key + " should be a scalar or a map"); \
        }                                                                                                       \
    })

    LOAD_MAP(CSourceCompiles);
    LOAD_MAP(CSourceRuns);
    LOAD_MAP(CXXSourceCompiles);
    LOAD_MAP(CXXSourceRuns);

    LOAD_MAP(Custom);
}

void Checks::load(const path &fn)
{
    load(YAML::LoadFile(fn.string()));
}

void Checks::save(yaml &root) const
{
    for (auto &c : checks)
    {
        auto &i = c->getInformation();
        auto t = i.type;

        switch (t)
        {
        case Check::Function:
        case Check::Type:
        case Check::Library:
            root[i.cppan_key].push_back(c->getData());
            break;
        case Check::Include:
        case Check::Symbol:
        case Check::CSourceCompiles:
        case Check::CSourceRuns:
        case Check::CXXSourceCompiles:
        case Check::CXXSourceRuns:
        case Check::Custom:
            c->save(root);
            break;
        }
    }
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

String Checks::save() const
{
    yaml root;
    save(root);
    return YAML::Dump(root);
}

void invert(Context &ctx, const CheckPtr &c)
{
    ctx.addLine();
    ctx.addLine("if (" + c->getVariable() + ")");
    ctx.addLine("    set(" + c->getVariable() + " 0)");
    ctx.addLine("else()");
    ctx.addLine("    set(" + c->getVariable() + " 1)");
    ctx.addLine("endif()");
}

void Checks::write_checks(Context &ctx) const
{
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
                    invert(ctx, c);
            }
            break;
        case Check::Custom:
            // Why not c->getDataEscaped()?
            // because user can write other cmake code that does not need escaping
            // user should provide escaping (in e.g. check_c_code_compiles) himself
            // note that such escaping is very tricky: '\' is '\\\\' when escaped
            ctx.addLine(c->getData());
            {
                auto p = (CheckSource *)c.get();
                if (p->invert)
                    invert(ctx, c);
            }
            break;
        default:
            throw std::logic_error("Write parallel check for type " + std::to_string(t) + " not implemented");
        }

        ctx.addLine("add_variable(" + c->getVariable() + ")");
        ctx.decreaseIndent();
        ctx.addLine("endif()");
        ctx.addLine();

        if (t == Check::Type)
        {
            CheckType ct(c->getData(), "SIZEOF_");
            CheckType ct_(c->getData(), "SIZE_OF_");

            ctx.addLine("if (" + c->getVariable() + ")");
            ctx.increaseIndent();
            ctx.addLine("set(" + ct_.getVariable() + " ${" + c->getVariable() + "} CACHE STRING \"\")");
            ctx.addLine("set(" + ct.getVariable() + " ${" + c->getVariable() + "} CACHE STRING \"\")");
            ctx.decreaseIndent();
            ctx.addLine("endif()");
            ctx.addLine();
        }
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
                    invert(ctx, c);
            }
            break;
        case Check::Custom:
            // Why not c->getDataEscaped()?
            // because user can write other cmake code that does not need escaping
            // user should provide escaping (in e.g. check_c_code_compiles) himself
            // note that such escaping is very tricky: '\' is '\\\\' when escaped
            ctx.addLine(c->getData());
            {
                auto p = (CheckSource *)c.get();
                if (p->invert)
                    invert(ctx, c);
            }
            break;
        default:
            throw std::logic_error("Write parallel check for type " + std::to_string(t) + " not implemented");
        }
        ctx.addLine("if (NOT " + c->getVariable() + ")");
        ctx.addLine("    set(" + c->getVariable() + " 0)");
        ctx.addLine("endif()");
        ctx.addLine("file(WRITE " + c->getVariable() + " \"${" + c->getVariable() + "}\")");
        ctx.addLine();
    }
}

void Checks::read_parallel_checks_for_workers(const path &dir)
{
    for (auto &c : checks)
    {
        auto s = read_file(dir / c->getVariable());
        boost::trim(s);
        if (!s.empty())
            c->setValue(std::stoi(s));
        else
        {
            c->setValue(0);
            LOG_INFO(logger, "Empty value for variable: " + c->getVariable());
        }
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

void Checks::remove_known_vars(const std::set<String> &known_vars)
{
    auto checks_old = checks;
    for (auto &c : checks_old)
    {
        if (known_vars.find(c->getVariable()) != known_vars.end())
            checks.erase(c);
    }
}

std::vector<Checks> Checks::scatter(int N) const
{
    std::vector<Checks> workers(N);
    int i = 0;
    for (auto &c : checks)
        workers[i++ % N].checks.insert(c);
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
        case Check::Symbol:
            if (c->getValue())
                LOG_INFO(logger, "-- " << i.singular << " " + c->getVariable() + " - found (" + std::to_string(c->getValue()) + ")");
            else
                LOG_INFO(logger, "-- " << i.singular << " " + c->getVariable() + " - not found");
            break;
        case Check::CSourceCompiles:
        case Check::CSourceRuns:
        case Check::CXXSourceCompiles:
        case Check::CXXSourceRuns:
        case Check::Custom:
        {
            auto cc = (CheckSource *)c.get();
            if ((!cc->invert && c->getValue()) ||
                (cc->invert && !c->getValue()))
                LOG_INFO(logger, "-- Test " << c->getVariable() + " - Success (" + std::to_string(c->getValue()) + ")");
            else
                LOG_INFO(logger, "-- Test " << c->getVariable() + " - Failed");
        }
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
