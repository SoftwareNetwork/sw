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

    { Check::LibraryFunction,
    { Check::LibraryFunction, "check_library_function", "check_library_exists", "library function", "functions" } },

    { Check::Symbol,
    { Check::Symbol, "check_symbol_exists", "check_cxx_symbol_exists", "symbol", "symbols" } },

    { Check::Alignment,
    { Check::Alignment, "check_type_alignment", "check_type_alignment", "alignment", "alignments" } },

    { Check::Decl,
    { Check::Decl, "check_decl_exists", "check_c_source_compiles", "declaration", "declarations" } },

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
    bool has_decl = false;

#define LOAD_SET(t)                                                                     \
    do                                                                                  \
    {                                                                                   \
        auto seq = get_sequence<String>(root, getCheckInformation(Check::t).cppan_key); \
        for (auto &v : seq)                                                             \
        {                                                                               \
            auto p = addCheck<Check##t>(v);                                             \
            if (p->getInformation().type == Check::Decl)                                \
                has_decl = true;                                                        \
        }                                                                               \
    } while (0)

    LOAD_SET(Function);
    LOAD_SET(Library);
    LOAD_SET(Type);
    LOAD_SET(Decl);
    LOAD_SET(Alignment);

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
            auto cpp = v["cpp"].template as<bool>();
            auto p = this->addCheck<CheckInclude>(f, var);
            p->set_cpp(cpp);
        }
    });

    // library functions
    get_sequence_and_iterate(root, getCheckInformation(Check::LibraryFunction).cppan_key, [this](const auto &v)
    {
        if (v.IsMap())
        {
            auto f = v["function"].template as<String>();
            auto lib = v["library"].template as<String>();
            auto p = this->addCheck<CheckLibraryFunction>(f, lib);
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

    // common checks

    // add some common types
    addCheck<CheckType>("size_t");
    addCheck<CheckType>("void *");

    if (has_decl)
    {
        // headers
        addCheck<CheckInclude>("sys/types.h");
        addCheck<CheckInclude>("sys/stat.h");
        addCheck<CheckInclude>("stdlib.h");
        addCheck<CheckInclude>("stddef.h");
        addCheck<CheckInclude>("memory.h");
        addCheck<CheckInclude>("string.h");
        addCheck<CheckInclude>("strings.h");
        addCheck<CheckInclude>("inttypes.h");
        addCheck<CheckInclude>("stdint.h");
        addCheck<CheckInclude>("unistd.h");

        // STDC_HEADERS
        addCheck<CheckCSourceCompiles>("STDC_HEADERS", R"(
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <float.h>
int main() {return 0;}
)");
    }
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
        case Check::Decl:
        case Check::Alignment:
            root[i.cppan_key].push_back(c->getData());
            break;
        case Check::LibraryFunction:
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
    return dump_yaml_config(root);
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
        case Check::Alignment:
            // for C language, can be opted later for C++
            ctx.addLine(i.function + "(\"" + c->getData() + "\" C " + c->getVariable() + ")");
            break;
        case Check::Library:
            ctx.addLine("find_library(" + c->getVariable() + " " + c->getData() + ")");
            ctx.addLine("if (\"${" + c->getVariable() + "}\" STREQUAL \"" + c->getVariable() + "-NOTFOUND\")");
            ctx.addLine("    set(" + c->getVariable() + " 0)");
            ctx.addLine("else()");
            ctx.addLine("    set(" + c->getVariable() + " 1)");
            ctx.addLine("endif()");
            break;
        case Check::LibraryFunction:
        {
            auto p = (CheckLibraryFunction *)c.get();
            ctx.addLine(i.function + "(" + p->library +" \"" + c->getData() + "\" \"\" " + c->getVariable() + ")");
        }
        break;
        case Check::Decl:
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

        ctx.addLine("add_check_variable(" + c->getVariable() + ")");
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
        case Check::Alignment:
            // for C language, can be opted later for C++
            ctx.addLine(i.function + "(\"" + c->getData() + "\" C " + c->getVariable() + ")");
            break;
        case Check::Library:
            ctx.addLine("find_library(" + c->getVariable() + " " + c->getData() + ")");
            ctx.addLine("if (\"${" + c->getVariable() + "}\" STREQUAL \"" + c->getVariable() + "-NOTFOUND\")");
            ctx.addLine("    set(" + c->getVariable() + " 0)");
            ctx.addLine("else()");
            ctx.addLine("    set(" + c->getVariable() + " 1)");
            ctx.addLine("endif()");
            break;
        case Check::LibraryFunction:
        {
            auto p = (CheckLibraryFunction *)c.get();
            ctx.addLine(i.function + "(" + p->library + " \"" + c->getData() + "\" \"\" " + c->getVariable() + ")");
        }
        break;
        case Check::Decl:
            continue; // do not participate in parallel
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
        auto fn = dir / c->getVariable();
        if (!fs::exists(fn))
            continue;
        auto s = read_file(fn);
        boost::trim(s);
        if (s.empty())
        {
            // if s empty, we do not read var
            // it will be checked in normal mode
            continue;
        }
        c->setValue(std::stoi(s));
    }
}

void Checks::write_definitions(Context &ctx, const Package &d) const
{
    String m = "INTERFACE";
    if (!d.flags[pfHeaderOnly])
        m = "PUBLIC";
    if (d.flags[pfExecutable])
        m = "PRIVATE";

    auto print_def = [&ctx, &m](const String &value, auto &&s)
    {
        ctx << m << " " << s << "=" << value << Context::eol;
        return 0;
    };

    auto add_if_definition = [&ctx, &print_def](const String &s, const String &value, auto && ... defs)
    {
        ctx.addLine("if (" + s + ")");
        ctx.increaseIndent();
        ctx.addLine("target_compile_definitions(${this}");
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

        if (t == Check::Decl)
        {
            // decl will be always defined
            ctx.addLine("if (NOT DEFINED " + c->getVariable() + ")");
            ctx.increaseIndent();
            ctx.addLine("set(" + c->getVariable() + " 0)");
            ctx.decreaseIndent();
            ctx.addLine("endif()");
            ctx.addLine();

            ctx.addLine("target_compile_definitions(${this}");
            ctx.increaseIndent();
            ctx << m << " " << c->getVariable() << "=" << "${" << c->getVariable() << "}" << Context::eol;
            ctx.decreaseIndent();
            ctx.addLine(")");
            continue;
        }

        String value = "1";

        if (t == Check::Alignment)
            value = "${" + c->getVariable() + "}";

        add_if_definition(c->getVariable(), value);

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
    {
        auto &inf = c->getInformation();
        auto t = inf.type;

        switch (t)
        {
        case Check::Decl: // do not participate in parallel
            break;
        default:
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
        case Check::Alignment:
        case Check::Library:
        case Check::LibraryFunction:
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
        case Check::Decl:
            break;
            if (c->getValue())
                LOG_INFO(logger, "-- " << i.singular << " " + c->getVariable() + " - found (" + std::to_string(c->getValue()) + ")");
            else
                LOG_INFO(logger, "-- " << i.singular << " " + c->getVariable() + " - not found (" + std::to_string(c->getValue()) + ")");
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
        auto &i = c->getInformation();
        auto t = i.type;

        switch (t)
        {
        case Check::Decl: // do not participate in parallel
            break;
        default:
            ctx.addLine("STRING;" + c->getVariable() + ";" + std::to_string(c->getValue()));
            break;
        }
    }
}
