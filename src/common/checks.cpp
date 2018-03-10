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

#include "checks.h"
#include "checks_detail.h"

#include "hash.h"
#include "printers/printer.h"

#include <boost/algorithm/string.hpp>

#include <iostream>
#include <memory>

#include <primitives/log.h>
//DECLARE_STATIC_LOGGER(logger, "checks");

const std::map<int, Check::Information> check_information{
    { Check::Function,
    {
        Check::Function,
        "check_function_exists",
        "check_function_exists",
        "function",
        "functions" } },

    { Check::Include,
    {
        Check::Include,
        "check_include_exists",
        "check_include_files",
        "include",
        "includes" } },

    { Check::Type,
    {
        Check::Type,
        "check_type_size",
        "check_type_size",
        "type",
        "types" } },

    { Check::Library,
    {
        Check::Library,
        "check_library_exists",
        "find_library",
        "library",
        "libraries" } },

    { Check::LibraryFunction,
    {
        Check::LibraryFunction,
        "check_library_function",
        "check_library_exists",
        "library function",
        "functions" } },

    { Check::Symbol,
    {
        Check::Symbol,
        "check_symbol_exists",
        "check_symbol_exists",
        "symbol",
        "symbols" } },

    { Check::StructMember,
    {
        Check::StructMember,
        "check_struct_member",
        "check_struct_has_member",
        "member",
        "members" } },

    { Check::Alignment,
    {
        Check::Alignment,
        "check_type_alignment",
        "check_type_alignment",
        "alignment",
        "alignments" } },

    { Check::Decl,
    {
        Check::Decl,
        "check_decl_exists",
        "check_c_source_compiles",
        "declaration",
        "declarations" } },

    { Check::CSourceCompiles,
    {
        Check::CSourceCompiles,
        "check_c_source_compiles",
        "check_c_source_compiles",
        "c_source_compiles",
        "c_source_compiles" } },

    { Check::CSourceRuns,
    {
        Check::CSourceRuns,
        "check_c_source_runs",
        "check_c_source_runs",
        "c_source_runs",
        "c_source_runs" } },

    { Check::CXXSourceCompiles,
    {
        Check::CXXSourceCompiles,
        "check_cxx_source_compiles",
        "check_cxx_source_compiles",
        "cxx_source_compiles",
        "cxx_source_compiles" } },

    { Check::CXXSourceRuns,
    {
        Check::CXXSourceRuns,
        "check_cxx_source_runs",
        "check_cxx_source_runs",
        "cxx_source_runs",
        "cxx_source_runs" } },

    { Check::Custom,
    {
        Check::Custom,
        "checks",
        "",
        "custom",
        "custom" } },
};

Check::Information getCheckInformation(int type)
{
    auto i = check_information.find(type);
    if (i == check_information.end())
        return Check::Information();
    return i->second;
}

Check::Check(const Information &i, const CheckParameters &parameters)
    : information(i), parameters(parameters)
{
}

String Check::getDataEscaped() const
{
    auto d = getData();
    boost::replace_all(d, "\\", "\\\\\\\\");
    boost::replace_all(d, "\"", "\\\"");
    return d;
}

void Checks::load(const path &fn)
{
    load(YAML::Load(read_file(fn)));
}

void Checks::load(const yaml &root)
{
    // functions
    get_sequence_and_iterate(root, getCheckInformation(Check::Function).cppan_key, [this](const auto &n)
    {
        if (n.IsScalar())
            this->addCheck<CheckFunction>(n.template as<String>());
        else if (n.IsMap())
        {
            String f;
            if (n["name"].IsDefined())
                f = n["name"].template as<String>();
            else if (n["function"].IsDefined())
                f = n["function"].template as<String>();
            CheckParameters p;
            p.load(n);
            auto ptr = this->addCheck<CheckFunction>(f, p);
            if (n["cpp"].IsDefined())
                ptr->set_cpp(n["cpp"].template as<bool>());
        }
    });

    // types
    get_sequence_and_iterate(root, getCheckInformation(Check::Type).cppan_key, [this](const auto &n)
    {
        if (n.IsScalar())
            this->addCheck<CheckType>(n.template as<String>());
        else if (n.IsMap())
        {
            if (n.size() == 1)
            {
                auto i = n.begin();
                auto t = i->first.template as<String>();
                auto h = i->second.template as<String>();
                CheckParameters p;
                // if we see onliner 'type: struct tm' interpret it as
                // type 'struct tm', not type 'type' and header 'struct tm'
                if (t == "type")
                    t = h;
                else
                    p.headers.push_back(h);
                this->addCheck<CheckType>(t, p);
                return;
            }
            String t;
            if (n["name"].IsDefined())
                t = n["name"].template as<String>();
            else if (n["type"].IsDefined())
                t = n["type"].template as<String>();
            CheckParameters p;
            p.load(n);
            auto ptr = this->addCheck<CheckType>(t, p);
            if (n["cpp"].IsDefined())
                ptr->set_cpp(n["cpp"].template as<bool>());
        }
    });

    // struct members
    get_sequence_and_iterate(root, getCheckInformation(Check::StructMember).cppan_key, [this](const auto &n)
    {
        if (n.IsMap())
        {
            if (n.size() == 1)
            {
                auto i = n.begin();
                auto m = i->first.template as<String>();
                auto s = i->second.template as<String>();
                this->addCheck<CheckStructMember>(m, s);
                return;
            }
            String m;
            if (n["name"].IsDefined())
                m = n["name"].template as<String>();
            else if (n["member"].IsDefined())
                m = n["member"].template as<String>();
            auto s = n["struct"].template as<String>();
            CheckParameters p;
            p.load(n);
            auto ptr = this->addCheck<CheckStructMember>(m, s, p);
            if (n["cpp"].IsDefined())
                ptr->set_cpp(n["cpp"].template as<bool>());
        }
        else
            throw std::runtime_error("struct member must be a map");
    });

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

    LOAD_SET(Library);
    LOAD_SET(Alignment);

    // decls
    const auto &decl_key = getCheckInformation(Check::Decl).cppan_key;
    if (root[decl_key].IsDefined())
    {
        has_decl = true;
        if (root[decl_key].IsMap())
        {
            get_map_and_iterate(root, decl_key, [this](const auto &root)
            {
                auto f = root.first.template as<String>();
                if (root.second.IsSequence() || root.second.IsScalar())
                {
                    CheckParameters p;
                    p.headers = get_sequence<String>(root.second);
                    this->addCheck<CheckDecl>(f, p);
                }
                else
                    throw std::runtime_error("Decl headers should be a scalar or a set");
            });
        }
        else if (root[decl_key].IsSequence())
        {
            get_sequence_and_iterate(root, decl_key, [this](const auto &n)
            {
                if (n.IsMap())
                {
                    if (n.size() == 1)
                    {
                        auto i = n.begin();
                        auto s = i->first.template as<String>();
                        auto h = i->second.template as<String>();
                        CheckParameters p;
                        p.headers = { h };
                        this->addCheck<CheckDecl>(s, p);
                        return;
                    }
                    String s;
                    if (n["name"].IsDefined())
                        s = n["name"].template as<String>();
                    else if (n["decl"].IsDefined())
                        s = n["decl"].template as<String>();
                    CheckParameters p;
                    p.load(n);
                    /*auto ptr = */this->addCheck<CheckDecl>(s, p);
                    return;
                }
                else if (n.IsScalar())
                {
                    this->addCheck<CheckDecl>(n.template as<String>());
                }
                else
                    throw std::runtime_error("decl must be a map or seq");
            });
        }
    }

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
            /*auto p = */this->addCheck<CheckLibraryFunction>(f, lib);
        }
    });

    // symbols
    const auto &skey = getCheckInformation(Check::Symbol).cppan_key;
    if (root[skey].IsDefined())
    {
        if (root[skey].IsMap())
        {
            get_map_and_iterate(root, skey, [this](const auto &root)
            {
                auto f = root.first.template as<String>();
                if (root.second.IsSequence() || root.second.IsScalar())
                {
                    CheckParameters p;
                    p.headers = get_sequence<String>(root.second);
                    this->addCheck<CheckSymbol>(f, p);
                }
                else
                    throw std::runtime_error("Symbol headers should be a scalar or a set");
            });
        }
        else if (root[skey].IsSequence())
        {
            get_sequence_and_iterate(root, skey, [this](const auto &n)
            {
                if (n.IsMap())
                {
                    if (n.size() == 1)
                    {
                        auto i = n.begin();
                        auto s = i->first.template as<String>();
                        auto h = i->second.template as<String>();
                        CheckParameters p;
                        p.headers = { h };
                        this->addCheck<CheckSymbol>(s, p);
                        return;
                    }
                    String s;
                    if (n["name"].IsDefined())
                        s = n["name"].template as<String>();
                    else if (n["symbol"].IsDefined())
                        s = n["symbol"].template as<String>();
                    CheckParameters p;
                    p.load(n);
                    auto ptr = this->addCheck<CheckSymbol>(s, p);
                    if (n["cpp"].IsDefined())
                        ptr->set_cpp(n["cpp"].template as<bool>());
                    return;
                }
                throw std::runtime_error("symbol must be a map");
            });
        }
    }

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

    // common (default) checks

    // add some common types
    addCheck<CheckType>("size_t")->default_ = true;
    addCheck<CheckType>("void *")->default_ = true;

    if (has_decl)
    {
        // headers
        addCheck<CheckInclude>("sys/types.h")->default_ = true;
        addCheck<CheckInclude>("sys/stat.h")->default_ = true;
        addCheck<CheckInclude>("stdlib.h")->default_ = true;
        addCheck<CheckInclude>("stddef.h")->default_ = true;
        addCheck<CheckInclude>("memory.h")->default_ = true;
        addCheck<CheckInclude>("string.h")->default_ = true;
        addCheck<CheckInclude>("strings.h")->default_ = true;
        addCheck<CheckInclude>("inttypes.h")->default_ = true;
        addCheck<CheckInclude>("stdint.h")->default_ = true;
        addCheck<CheckInclude>("unistd.h")->default_ = true;

        // STDC_HEADERS
        addCheck<CheckCSourceCompiles>("STDC_HEADERS", R"(
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <float.h>
int main() {return 0;}
)")->default_ = true;
    }
}

void Checks::save(yaml &root) const
{
    for (auto &c : checks)
    {
        if (c->default_)
            continue;

        auto &i = c->getInformation();
        auto t = i.type;

        switch (t)
        {
        case Check::Library:
        case Check::Alignment:
            root[i.cppan_key].push_back(c->getData());
            break;
        case Check::Decl:
        case Check::Type:
        case Check::Function:
        case Check::LibraryFunction:
        case Check::Include:
        case Check::Symbol:
        case Check::StructMember:
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

void invert(CMakeContext &ctx, const CheckPtr &c)
{
    ctx.addLine();
    ctx.addLine("if (" + c->getVariable() + ")");
    ctx.addLine("    set(" + c->getVariable() + " 0)");
    ctx.addLine("else()");
    ctx.addLine("    set(" + c->getVariable() + " 1)");
    ctx.addLine("endif()");
}

void Checks::write_checks(CMakeContext &ctx, const StringSet &prefixes) const
{
    for (auto &c : checks)
    {
        auto &i = c->getInformation();
        auto t = i.type;

        ctx.if_("NOT DEFINED " + c->getVariable());

        switch (t)
        {
        case Check::Include:
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
        case Check::Function:
        case Check::Symbol:
        case Check::StructMember:
        case Check::Type:
        case Check::Decl:
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
        ctx.endif();

        for (const auto &p : prefixes)
        {
            ctx.addLine("set(" + p + c->getVariable() + " ${" + c->getVariable() + "} CACHE STRING \"\")");
            ctx.addLine("set(" + p + boost::to_lower_copy(c->getVariable()) + " ${" + c->getVariable() + "} CACHE STRING \"\")");
        }

        ctx.emptyLines();

        if (t == Check::Symbol)
        {
            auto f = (CheckSymbol*)c.get();
            if (!f->parameters.headers.empty())
            {
                ctx.addLine("if (" + c->getVariable() + ")");
                ctx.increaseIndent();
                for (auto &i : f->parameters.headers)
                {
                    auto iv = Check::make_include_var(i);
                    ctx.addLine("set(" + iv + " 1 CACHE STRING \"\")");
                    for (const auto &p : prefixes)
                    {
                        ctx.addLine("set(" + p + iv + " ${" + iv + "} CACHE STRING \"\")");
                        ctx.addLine("set(" + p + boost::to_lower_copy(iv) + " ${" + iv + "} CACHE STRING \"\")");
                    }
                    ctx.addLine("add_check_variable(" + iv + ")");
                }
                ctx.decreaseIndent();
                ctx.addLine("endif()");
                ctx.addLine();
            }
        }

        if (t == Check::Type)
        {
            CheckType ct(c->getData(), "SIZEOF_");
            CheckType ct_(c->getData(), "SIZE_OF_");

            ctx.addLine("if (" + c->getVariable() + ")");
            ctx.increaseIndent();
            ctx.addLine("set(" + ct_.getVariable() + " ${" + c->getVariable() + "} CACHE STRING \"\")");
            ctx.addLine("set(" + ct.getVariable() + " ${" + c->getVariable() + "} CACHE STRING \"\")");
            for (const auto &p : prefixes)
            {
                ctx.addLine("set(" + p + ct_.getVariable() + " ${" + c->getVariable() + "} CACHE STRING \"\")");
                ctx.addLine("set(" + p + ct.getVariable() + " ${" + c->getVariable() + "} CACHE STRING \"\")");
                ctx.addLine("set(" + p + boost::to_lower_copy(ct_.getVariable()) + " ${" + c->getVariable() + "} CACHE STRING \"\")");
                ctx.addLine("set(" + p + boost::to_lower_copy(ct.getVariable()) + " ${" + c->getVariable() + "} CACHE STRING \"\")");
            }
            ctx.decreaseIndent();
            ctx.addLine("endif()");
            ctx.addLine();
        }
    }
}

void Checks::write_parallel_checks_for_workers(CMakeContext &ctx) const
{
    for (auto &c : checks)
    {
        auto &i = c->getInformation();
        auto t = i.type;
        switch (t)
        {
        case Check::Include:
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
        case Check::Function:
        case Check::Symbol:
        case Check::StructMember:
        case Check::Type:
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
        ctx.increaseIndent();
        ctx.addLine("set(" + c->getVariable() + " 0)");
        ctx.decreaseIndent();
        ctx.addLine("else()");
        ctx.increaseIndent();

        if (t == Check::Symbol)
        {
            auto f = (CheckSymbol*)c.get();
            if (!f->parameters.headers.empty())
            {
                for (auto &i : f->parameters.headers)
                {
                    auto iv = Check::make_include_var(i);
                    ctx.addLine("file(WRITE " + iv + " \"1\")");
                }
            }
        }

        ctx.decreaseIndent();
        ctx.addLine("endif()");
        ctx.addLine("file(WRITE " + c->getFileName() + " \"${" + c->getVariable() + "}\")");
        ctx.addLine();
    }
}

void Checks::read_parallel_checks_for_workers(const path &dir)
{
    for (auto &c : checks)
    {
        auto fn = dir / c->getFileName();
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

void Checks::write_definitions(CMakeContext &ctx, const Package &d, const StringSet &prefixes) const
{
    const auto m = [&d]
    {
        if (!d.flags[pfHeaderOnly])
            return "PUBLIC"s;
        if (d.flags[pfExecutable])
            return "PRIVATE"s;
        return "INTERFACE"s;
    }();

    auto print_def = [&ctx, &m, &prefixes](const auto &value, const auto &s)
    {
        ctx.addLine(m + " " + s + "=" + value);
        for (const auto &p : prefixes)
            ctx.addLine(m + " " + p + s + "=" + value);
        return 0;
    };

    auto add_if_definition = [&ctx, &print_def](const String &s, const String &value, const std::vector<String> &defs = std::vector<String>())
    {
        ctx.if_(s);
        ctx.addLine("target_compile_definitions(${this}");
        ctx.increaseIndent();
        print_def(value, s);
        for (auto &def : defs)
            print_def(value, def);
        ctx.decreaseIndent();
        ctx.addLine(")");
        ctx.endif();
        ctx.addLine();
    };

    // aliases
    add_if_definition("WORDS_BIGENDIAN", "1", {"BIGENDIAN", "BIG_ENDIAN", "HOST_BIG_ENDIAN"});

    for (auto &c : checks)
    {
        auto &i = c->getInformation();
        auto t = i.type;

        if (t == Check::Decl)
        {
            // decl will be always defined
            // TODO: watch over this condition, it fails sometimes
            ctx.addLine("if (NOT DEFINED " + c->getVariable() + " OR NOT " + c->getVariable() + ")");
            ctx.increaseIndent();
            ctx.addLine("set(" + c->getVariable() + " 0)");
            ctx.decreaseIndent();
            ctx.addLine("endif()");
            ctx.addLine();

            ctx.addLine("target_compile_definitions(${this}");
            ctx.increaseIndent();
            ctx.addLine(m + " " + c->getVariable() + "=" + "${" + c->getVariable() + "}");
            for (const auto &p : prefixes)
                ctx.addLine(m + " " + p + c->getVariable() + "=" + "${" + c->getVariable() + "}");
            ctx.decreaseIndent(")");
            ctx.addLine();
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
    std::map<String, CheckPtr> checks_to_print;
    for (auto &c : checks)
    {
        auto &i = c->getInformation();
        auto t = i.type;

        // skip decls
        if (t == Check::Decl)
            continue;

        // if we have duplicate values, choose the ok one
        auto &m = checks_to_print[c->getVariable()];
        if (m && m->isOk())
            continue;
        m = c;
    }

    // correctly sort
    ChecksSet s;
    for (auto &kv : checks_to_print)
        s.insert(kv.second);
    for (auto &v : s)
        std::cout << v->printStatus() << std::endl;
        //LOG_INFO(logger, v->printStatus());
}

void Checks::print_values(CMakeContext &ctx) const
{
    std::map<String, CheckPtr> checks_to_print;
    for (auto &c : checks)
    {
        auto &i = c->getInformation();
        auto t = i.type;

        switch (t)
        {
        case Check::Decl: // do not participate in parallel
            break;
        case Check::Type:
        {
            auto &m = checks_to_print[c->getVariable()];
            if (m && m->isOk())
                continue;
            m = c;
            checks_to_print[Check::make_type_var(c->getData(), "SIZEOF_")] = c;
            checks_to_print[Check::make_type_var(c->getData(), "SIZE_OF_")] = c;
            break;
        }
        case Check::Symbol:
            if (c->isOk())
            {
                // add headers as found directly to ctx
                auto f = (CheckSymbol*)c.get();
                for (auto &i : f->parameters.headers)
                    ctx.addLine("STRING;" + Check::make_include_var(i) + ";1");
            }
            //[[fallthrough]];
        default:
        {
            // if we have duplicate values, choose the ok one
            auto &m = checks_to_print[c->getVariable()];
            if (m && m->isOk())
                continue;
            m = c;
            break;
        }
        }
    }

    for (auto &kv : checks_to_print)
        ctx.addLine("STRING;" + kv.first + ";" + std::to_string(kv.second->getValue()));
}

String Check::make_include_var(const String &i)
{
    auto v_def = "HAVE_" + boost::algorithm::to_upper_copy(i);
    for (auto &c : v_def)
    {
        if (!isalnum(c))
            c = '_';
    }
    return v_def;
}

String Check::make_type_var(const String &t, const String &prefix)
{
    String v_def = prefix;
    v_def += boost::algorithm::to_upper_copy(t);
    for (auto &c : v_def)
    {
        if (c == '*')
            c = 'P';
        else if (!isalnum(c))
            c = '_';
    }
    return v_def;
}

String Check::make_struct_member_var(const String &m, const String &s)
{
    return make_include_var(s + " " + m);
}

String Check::getFileName() const
{
    if (parameters.empty())
        return getVariable();
    return getVariable() + "_" + parameters.getHash();
}

String CheckParameters::getHash() const
{
    String h;
#define ADD_PARAMS(x) for (auto &v : x) h += v
    ADD_PARAMS(headers);
    ADD_PARAMS(definitions);
    ADD_PARAMS(include_directories);
    ADD_PARAMS(libraries);
    ADD_PARAMS(flags);
    h = sha256(h);
    h = h.substr(0, 4);
    return h;
}

void CheckParameters::writeHeadersBefore(CMakeContext &ctx) const
{
    if (!headers.empty())
    {
        ctx.addLine("set(_oh ${CMAKE_EXTRA_INCLUDE_FILES})");
        ctx.addLine("set(CMAKE_EXTRA_INCLUDE_FILES");
        for (auto &d : headers)
            ctx.addLine(d);
        ctx.addLine(")");
    }
}

void CheckParameters::writeHeadersAfter(CMakeContext &ctx) const
{
    if (!headers.empty())
        ctx.addLine("set(CMAKE_EXTRA_INCLUDE_FILES ${_oh})");
}

void CheckParameters::writeBefore(CMakeContext &ctx) const
{
    if (!definitions.empty())
    {
        ctx.addLine("set(_od ${CMAKE_REQUIRED_DEFINITIONS})");
        ctx.addLine("set(CMAKE_REQUIRED_DEFINITIONS");
        for (auto &d : definitions)
            ctx.addLine(d);
        ctx.addLine(")");
    }
    if (!include_directories.empty())
    {
        ctx.addLine("set(_oi ${CMAKE_REQUIRED_INCLUDES})");
        ctx.addLine("set(CMAKE_REQUIRED_INCLUDES");
        for (auto &d : include_directories)
            ctx.addLine(d);
        ctx.addLine(")");
    }
    if (!libraries.empty())
    {
        ctx.addLine("set(_ol ${CMAKE_REQUIRED_LIBRARIES})");
        ctx.addLine("set(CMAKE_REQUIRED_LIBRARIES");
        for (auto &d : libraries)
            ctx.addLine(d);
        ctx.addLine(")");
    }
    if (!flags.empty())
    {
        ctx.addLine("set(_of ${CMAKE_REQUIRED_FLAGS})");
        ctx.addLine("set(CMAKE_REQUIRED_FLAGS");
        for (auto &d : flags)
            ctx.addLine(d);
        ctx.addLine(")");
    }
}

void CheckParameters::writeAfter(CMakeContext &ctx) const
{
    if (!definitions.empty())
        ctx.addLine("set(CMAKE_REQUIRED_DEFINITIONS ${_od})");
    if (!include_directories.empty())
        ctx.addLine("set(CMAKE_REQUIRED_INCLUDES    ${_oi})");
    if (!libraries.empty())
        ctx.addLine("set(CMAKE_REQUIRED_LIBRARIES   ${_ol})");
    if (!flags.empty())
        ctx.addLine("set(CMAKE_REQUIRED_FLAGS       ${_of})");
}

void CheckParameters::load(const yaml &n)
{
    headers = get_sequence<String>(n["headers"]);
    definitions = get_sequence_set<String>(n["definitions"]);
    include_directories = get_sequence_set<String>(n["include_directories"]);
    libraries = get_sequence_set<String>(n["libraries"]);
    flags = get_sequence_set<String>(n["flags"]);
}

void CheckParameters::save(yaml &n) const
{
#define ADD_SET(x) for (auto &v : x) n[#x].push_back(v)
    ADD_SET(headers);
    ADD_SET(definitions);
    ADD_SET(include_directories);
    ADD_SET(libraries);
    ADD_SET(flags);
}

bool CheckParameters::empty() const
{
    return
        headers.empty() &&
        definitions.empty() &&
        include_directories.empty() &&
        libraries.empty() &&
        flags.empty() &&
        1
        ;
}

bool CheckParameters::operator<(const CheckParameters &p) const
{
    return
        std::tie(headers, definitions, include_directories, libraries, flags) <
        std::tie(p.headers, p.definitions, p.include_directories, p.libraries, p.flags);
}
