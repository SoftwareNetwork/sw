/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2020 Egor Pugin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "checks.h"

#include <primitives/emitter.h>
#include <primitives/filesystem.h>

#include <boost/algorithm/string.hpp>

#include <iostream>
#include <regex>
#include <vector>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "sw.cli.autotools");

struct command
{
    String name;
    Strings params;
};

struct if_expr
{
    struct if_action
    {
        enum sign_type
        {
            S_UNK,

            S_EQ,
            S_NE,
            S_LT,
            S_GT,
        };

        String var;
        int sign;
        String value;
        String action;
        int start;
    };

    if_action if_actions;
    std::vector<if_action> if_else_actions;
    String else_actions;
};

struct ac_processor
{
    using value = std::pair<String, bool>;

    String file;
    std::vector<command> commands;
    sw::CheckSet1 checks;
    std::map<String, std::set<value>> vars;
    std::map<String, if_expr> conditions;
    bool cpp = false;

    ac_processor(const path &p);

    template <class T>
    auto split_and_add(command &c, std::function<bool(String)> fun = std::function<bool(String)>());
    template <class T>
    T *ifdef_add(command &c);
    template <class T>
    T *try_add(command &c);

    void output2();
    void process();
    void process_AC_LANG(command &c);
    void process_AC_CHECK_FUNCS(command &c);
    void process_AC_CHECK_DECLS(command &c);
    void process_AC_COMPILE_IFELSE(command &c);
    void process_AC_RUN_IFELSE(command &c);
    void process_AC_TRY_COMPILE(command &c);
    void process_AC_TRY_LINK(command &c);
    void process_AC_TRY_RUN(command &c);
    void process_AC_CHECK_HEADER(command &c);
    void process_AC_CHECK_HEADERS(command &c);
    void process_AC_CHECK_TYPES(command &c);
    void process_AC_HEADER_DIRENT(command &c);
    void process_AC_STRUCT_DIRENT_D_TYPE(command &c);
    void process_AC_HEADER_TIME(command &c);
    void process_AC_HEADER_ASSERT(command &c);
    void process_AC_HEADER_STDC(command &c);
    void process_AC_HEADER_MAJOR(command &c);
    void process_AC_HEADER_SYS_WAIT(command &c);
    void process_AC_HEADER_STDBOOL(command &c);
    void process_AC_STRUCT_TM(command &c);
    void process_AC_STRUCT_TIMEZONE(command &c);
    void process_AC_CHECK_LIB(command &c);
    void process_AC_CHECK_LIBM(command &c);
    void process_AC_CHECK_MEMBERS(command &c);
    void process_AC_DEFINE(command &c);
    void process_AC_CHECK_ALIGNOF(command &c);
    void process_AC_CHECK_SYMBOL(command &c);
};

static int get_end_of_string_block(const String &s, int i = 1)
{
    auto c = s[i - 1];
    int n_curly = c == '(';
    int n_square = c == '[';
    int n_quotes = c == '\"';
    auto sz = (int)s.size();
    while ((n_curly > 0 || n_square > 0 || n_quotes > 0) && i < sz)
    {
        c = s[i];

        if (c == '\"')
        {
            if (i && s[i - 1] == '\\')
                ;
            else if (n_quotes == 0)
                i = get_end_of_string_block(s, i + 1) - 1;
            else
                n_quotes--;
        }
        else
        {
            switch (c)
            {
            case '(':
            case '[':
                i = get_end_of_string_block(s, i + 1) - 1;
                break;
            case ')':
                n_curly--;
                break;
            case ']':
                n_square--;
                break;
            }
        }

        i++;
    }
    return i;
}

static auto parse_arguments(const String &f)
{
    int start = 0;
    int i = 0;
    int sz = (int)f.size();
    Strings s;

    auto add_to_s = [&s](auto str)
    {
        boost::trim(str);
        if (str.empty())
            return;
        while (!str.empty() && str.front() == '[' && str.back() == ']')
            str = str.substr(1, str.size() - 2);
        s.push_back(str);
    };

    while (i < sz)
    {
        auto c = f[i];
        switch (c)
        {
        case '\"':
            i = get_end_of_string_block(f, i + 1) - 1;
            break;
        case ',':
            add_to_s(f.substr(start, i - start));
            start = i + 1;
            break;
        case '(':
        case '[':
            i = get_end_of_string_block(f, i + 1) - 1;
            add_to_s(f.substr(start, i - start + 1));
            start = i + 1;
            break;
        }
        i++;
    }
    add_to_s(f.substr(start, i - start));
    return s;
}

static auto parse_command(const String &f)
{
    auto i = get_end_of_string_block(f.c_str());
    auto s = f.substr(1, i - 2);
    boost::trim(s);
    return parse_arguments(s);
}

static auto parse_configure_ac(String f)
{
    auto ac = {
        "AC_LANG",
        "AC_CHECK_\\w+",
        //"AC_EGREP_\\w+",
        "AC_TRY_\\w+",
        "AC_\\w+?_IFELSE",
        "AC_HEADER_\\w+",
        "AC_STRUCT_\\w+",
        "\nAC_DEFINE",
        "AC_FUNC_\\w+",
        "AC_TYPE_\\w+",
    };

    String rx = "(";
    for (auto &a : ac)
    {
        rx += a;
        rx += "|";
    }
    rx.resize(rx.size() - 1);
    rx += ")";

    std::regex r(rx);
    std::smatch m;
    std::vector<command> commands;
    while (std::regex_search(f, m, r))
    {
        command cmd;
        cmd.name = m[1].str();
        boost::trim(cmd.name);
        f = m.suffix();
        if (f[0] == '(')
            cmd.params = parse_command(f);
        commands.push_back(cmd);
    }
    return commands;
}

static auto parse_conditions(String f)
{
    std::map<String, if_expr> conds;

    String s = f;
    std::regex r_if(R"rrr(\sif\s+test\s+"?\$(\w+)"?\s+(\S+)\s+(\w+)\s*;?\s*then)rrr");
    std::smatch m;
    while (std::regex_search(s, m, r_if))
    {
        auto var = m[1].str();
        auto sign = m[2].str();
        auto val = m[3].str();

        if_expr::if_action a;
        a.var = var;
        a.value = val;
        a.start = (int)(m[0].first - s.begin());

        if (sign == "=")
            a.sign = if_expr::if_action::S_EQ;
        else if (sign == "!=")
            a.sign = if_expr::if_action::S_NE;
        else if (sign == "-lt")
            a.sign = if_expr::if_action::S_LT;
        else if (sign == "-gt")
            a.sign = if_expr::if_action::S_GT;
        else
            a.sign = if_expr::if_action::S_UNK;

        if (a.sign == if_expr::if_action::S_UNK) // TODO: implement other signs (-le, -ge)
            std::cerr << "Unknown sign " << sign << "\n";
        else
        {
            auto p = m[0].second - s.begin();
            auto f = s.find("fi", p);
            a.action = s.substr(p, f - p);
            boost::trim(a.action);
            conds[var].if_actions = a;
        }
        s = m.suffix();
    }

    return conds;
}

void process_configure_ac2(const path &p)
{
    ac_processor proc(p);
    proc.process();
    proc.output2();
}

ac_processor::ac_processor(const path &p)
{
    file = read_file(p);

    std::regex dnl("dnl.*?\\n");
    file = std::regex_replace(file, dnl, "\n");

    commands = parse_configure_ac(file);
    conditions = parse_conditions(file);
}

static void print_checks2(primitives::CppEmitter &ctx, const sw::CheckSet1 &checks, const String &name)
{
    ctx.beginBlock("void check(Checker &c)");
    ctx.addLine("auto &s = c.addSet(\"" + name + "\");");
    for (auto &c : checks.all)
    {
        switch (c->getType())
        {
        case sw::CheckType::Function:
            ctx.addLine("s.checkFunctionExists(\"" + c->getData() + "\");");
            break;
        case sw::CheckType::Include:
            ctx.addLine("s.checkIncludeExists(\"" + c->getData() + "\");");
            //if (c->get_cpp())
            break;
        case sw::CheckType::Type:
            ctx.addLine("s.checkTypeSize(\"" + c->getData() + "\");");
            break;
        case sw::CheckType::Declaration:
            ctx.addLine("s.checkDeclarationExists(\"" + c->getData() + "\");");
            break;
        case sw::CheckType::TypeAlignment:
            ctx.addLine("s.checkTypeAlignment(\"" + c->getData() + "\");");
            break;
        case sw::CheckType::LibraryFunction:
            ctx.addLine("s.checkLibraryFunctionExists(\"" + ((sw::LibraryFunctionExists*)c.get())->library + "\", \"" + c->getData() + "\");");
            break;
        case sw::CheckType::SourceCompiles:
            if (!c->Definitions.empty())
                ctx.addLine("s.checkSourceCompiles(\"" + *c->Definitions.begin() + "\", R\"sw_xxx(" + c->getData() + ")sw_xxx\");");
            else
                LOG_ERROR(logger, "no def for " + c->getData());
            break;
        case sw::CheckType::StructMember:
            ctx.beginBlock();
            ctx.addLine("auto &c = s.checkStructMemberExists(\"" + ((sw::StructMemberExists*)c.get())->struct_ + "\", \"" + c->getData() + "\");");
            for (auto &i : c->Parameters.Includes)
                ctx.addLine("c.Parameters.Includes.push_back(\"" + i + "\");");
            ctx.endBlock();
            break;
        case sw::CheckType::Symbol:
            ctx.beginBlock();
            ctx.addLine("auto &c = s.checkSymbolExists(\"" + c->getData() + "\");");
            for (auto &i : c->Parameters.Includes)
                ctx.addLine("c.Parameters.Includes.push_back(\"" + i + "\");");
            ctx.endBlock();
            break;
        }
    }
    ctx.endBlock();
}

void ac_processor::output2()
{
    primitives::CppEmitter ctx;
    print_checks2(ctx, checks, "x");
    std::cout << ctx.getText();
}

void prepare_type(String &t)
{
    if (t == "long_long")
        t = "long long";
}

void ac_processor::process()
{
    std::set<String> unproc;
    for (auto &c : commands)
    {

#define CASE(n1, n2)     \
    if (c.name == #n1)   \
    {                    \
        process_##n2(c); \
        continue;        \
    }

#define CASE_NOT_EMPTY(n1, n2) \
    if (c.name == #n1)         \
    {                          \
        if (c.params.empty())  \
            continue;          \
        process_##n2(c);       \
        continue;              \
    }

#define TWICE(m,x) m(x,x)

#define SILENCE(n1)    \
    if (c.name == #n1) \
    {                  \
        continue;      \
    }

        CASE_NOT_EMPTY(AC_CHECK_FUNCS_ONCE, AC_CHECK_FUNCS);
        CASE_NOT_EMPTY(AC_CHECK_FUNC, AC_CHECK_FUNCS);
        TWICE(CASE_NOT_EMPTY, AC_CHECK_FUNCS);

        CASE_NOT_EMPTY(AC_CHECK_DECL, AC_CHECK_DECLS);
        TWICE(CASE_NOT_EMPTY, AC_CHECK_DECLS);

        CASE_NOT_EMPTY(AC_COMPILE_IFELSE, AC_COMPILE_IFELSE);
        CASE_NOT_EMPTY(AC_LINK_IFELSE, AC_COMPILE_IFELSE);
        CASE_NOT_EMPTY(AC_PREPROC_IFELSE, AC_COMPILE_IFELSE);
        CASE_NOT_EMPTY(AC_TRY_CPP, AC_COMPILE_IFELSE); // AC_TRY_CPP is an obsolete of AC_PREPROC_IFELSE
        TWICE(CASE_NOT_EMPTY, AC_COMPILE_IFELSE);

        TWICE(CASE_NOT_EMPTY, AC_RUN_IFELSE);

        TWICE(CASE_NOT_EMPTY, AC_TRY_COMPILE);
        TWICE(CASE_NOT_EMPTY, AC_TRY_LINK);
        TWICE(CASE_NOT_EMPTY, AC_TRY_RUN);

        TWICE(CASE_NOT_EMPTY, AC_CHECK_HEADER);
        CASE_NOT_EMPTY(AC_CHECK_HEADERS_ONCE, AC_CHECK_HEADERS);
        TWICE(CASE_NOT_EMPTY, AC_CHECK_HEADERS);

        CASE_NOT_EMPTY(AC_CHECK_SIZEOF, AC_CHECK_TYPES);
        CASE_NOT_EMPTY(AC_CHECK_TYPE, AC_CHECK_TYPES);
        TWICE(CASE_NOT_EMPTY, AC_CHECK_TYPES);

        TWICE(CASE, AC_STRUCT_DIRENT_D_TYPE);
        TWICE(CASE, AC_HEADER_DIRENT);
        TWICE(CASE, AC_HEADER_TIME);
        TWICE(CASE, AC_HEADER_ASSERT);
        TWICE(CASE, AC_HEADER_STDC);
        TWICE(CASE, AC_HEADER_MAJOR);
        TWICE(CASE, AC_HEADER_SYS_WAIT);
        TWICE(CASE, AC_HEADER_STDBOOL);
        CASE_NOT_EMPTY(AC_CHECK_HEADER_STDBOOL, AC_HEADER_STDBOOL);

        TWICE(CASE, AC_STRUCT_TM);
        TWICE(CASE, AC_STRUCT_TIMEZONE);

        TWICE(CASE_NOT_EMPTY, AC_CHECK_LIB);
        TWICE(CASE, AC_CHECK_LIBM);

        CASE_NOT_EMPTY(AC_CHECK_MEMBER, AC_CHECK_MEMBERS);
        TWICE(CASE_NOT_EMPTY, AC_CHECK_MEMBERS);

        TWICE(CASE_NOT_EMPTY, AC_DEFINE);

        TWICE(CASE_NOT_EMPTY, AC_LANG);

        TWICE(CASE_NOT_EMPTY, AC_CHECK_ALIGNOF);
        TWICE(CASE_NOT_EMPTY, AC_CHECK_SYMBOL);

        SILENCE(AC_CHECK_PROG);
        SILENCE(AC_CHECK_PROGS);
        SILENCE(AC_CHECK_TOOLS);
        SILENCE(AC_CHECK_FILE);
        SILENCE(AC_CHECK_TOOL);
        SILENCE(AC_MSG_ERROR);
        SILENCE(AC_MSG_FAILURE);
        SILENCE(AC_TRY_COMMAND);

        // specific checks
        {
            std::regex r("AC_FUNC_(\\w+)");
            std::smatch m;
            if (std::regex_match(c.name, m, r))
            {
                auto v = m[1].str();
                boost::to_lower(v);
                checks.add<sw::FunctionExists>(v);
                continue;
            }
        }

        {
            std::regex r("AC_TYPE_(\\w+)");
            std::smatch m;
            if (std::regex_match(c.name, m, r))
            {
                auto v = m[1].str();
                boost::to_lower(v);
                prepare_type(v);
                checks.add<sw::TypeSize>(v);
                continue;
            }
        }

        if (unproc.find(c.name) == unproc.end())
        {
            std::cerr << "Unprocessed statement: " << c.name << "\n";
            unproc.insert(c.name);
        }
    }
}

template <class T>
auto ac_processor::split_and_add(command &c, std::function<bool(String)> fun)
{
    boost::replace_all(c.params[0], "\\", "\n");
    boost::replace_all(c.params[0], "\t", "\n");
    boost::replace_all(c.params[0], " ", "\n");
    boost::replace_all(c.params[0], ",", "\n");
    auto funcs = split_lines(c.params[0]);
    std::vector<T*> out;
    for (auto &f : funcs)
    {
        if (!fun || fun(f))
        {
            if (f == "snprintf")
            {
                auto c = checks.add<sw::SymbolExists>(f);
                c->Parameters.Includes = { "stdio.h" };
                continue;
            }
            if constexpr (std::is_same_v<T, sw::TypeSize>)
                prepare_type(f);
            out.push_back(checks.add<T>(f).get());
        }
    }
    return out;
}

template <class T>
T *ac_processor::ifdef_add(command &c)
{
    static std::regex r_kv("[\\d\\w-]+=[\\d\\w-]+");

    String var;
    String input = c.params[0];
    bool invert = false;

    if (c.params[0].find("AC_") == 0)
    {
        auto cmd = c.params[0].substr(0, c.params[0].find('('));
        if (cmd == "AC_LANG_PROGRAM")
        {
            auto params = parse_arguments(c.params[0].substr(cmd.size() + 1));
            input = params[0] + "\n\n int main() { \n\n";
            if (params.size() > 1)
                input += params[1];
            input += "\n\n ; return 0; }";
        }
        else if (cmd == "AC_MSG_RESULT")
            ; // this is a printer
        else if (cmd == "AC_MSG_ERROR")
            ; // this is a printer
        else if (cmd == "AC_MSG_FAILURE")
            ; // this is a printer
        else if (cmd == "AC_LANG_SOURCE")
        {
            auto params = parse_arguments(c.params[0].substr(cmd.size() + 1));
            input = params[0];
        }
        else if (cmd == "AC_LANG_CALL")
        {
            auto params = parse_arguments(c.params[0].substr(cmd.size() + 1));
            input = params[0] + "\n\n int main() { \n\n";
            if (params.size() > 1)
                input += params[1] + "()";
            input += "\n\n ; return 0; }";
        }
        else
        {
            std::cerr << "Unhandled AC_ statement: " << cmd << "\n";
            return nullptr;
        }
    }
    if (c.params.size() > 1)
    {
        if (c.params[1].find("AC_") == 0)
        {
            auto cmd = c.params[1].substr(0, c.params[1].find('('));
            if (cmd == "AC_MSG_RESULT")
                ; // this is a printer
            else if (cmd == "AC_MSG_ERROR")
                ; // this is a printer
            else if (cmd == "AC_MSG_FAILURE")
                ; // this is a printer
            else if (cmd == "AC_DEFINE")
            {
                auto params = parse_arguments(c.params[1].substr(cmd.size() + 1));
                var = params[0];
            }
            else
            {
                std::cerr << "Unhandled AC_ statement: " << cmd << "\n";
                return nullptr;
            }
        }
        else
        {
            // if key-value?
            if (std::regex_match(c.params[1], r_kv))
            {
                auto p = c.params[1].find('=');
                auto key = c.params[1].substr(0, p);
                auto value = c.params[1].substr(p + 1);
                vars[key].insert({ value, true });

                if (conditions.count(key))
                {
                    auto act = conditions[key].if_actions;

                    boost::replace_all(act.action, "\r", "");
                    boost::replace_all(act.action, "then", "\r");
                    Strings ifthen;
                    boost::split(ifthen, act.action, boost::is_any_of("\r"));

                    boost::trim(ifthen[0]);
                    if (ifthen.size() > 1)
                        boost::trim(ifthen[1]);

                    if (ifthen[0].find("AC_DEFINE") == 0)
                    {
                        auto cmd = ifthen[0].substr(0, ifthen[0].find('('));
                        auto params = parse_arguments(ifthen[0].substr(cmd.size() + 1));
                        var = params[0];

                        if (value == act.value)
                            invert = act.sign == if_expr::if_action::S_NE;
                        else
                            invert = act.sign == if_expr::if_action::S_EQ;
                    }

                    if (ifthen.size() > 1)
                    {
                        if (ifthen[0].find("AC_DEFINE") == 0)
                        {
                            auto cmd = ifthen[1].substr(0, ifthen[1].find('('));
                            auto params = parse_arguments(ifthen[1].substr(cmd.size() + 1));
                            var = params[0];
                        }

                        if (value == act.value)
                            invert = act.sign == if_expr::if_action::S_EQ;
                        else
                            invert = act.sign == if_expr::if_action::S_NE;
                    }
                }
            }
            else
                return nullptr;
        }
    }
    if (c.params.size() > 2)
    {
        if (c.params[2].find("AC_") == 0)
        {
            auto cmd = c.params[2].substr(0, c.params[2].find('('));
            if (cmd == "AC_MSG_RESULT")
                ; // this is a printer
            else if (cmd == "AC_MSG_ERROR")
                ; // this is a printer
            else if (cmd == "AC_MSG_FAILURE")
                ; // this is a printer
            else if (cmd == "AC_DEFINE")
            {
                auto params = parse_arguments(c.params[2].substr(cmd.size() + 1));
                //var = params[0]; // this could be very dangerous
            }
            else if (cmd == "AC_COMPILE_IFELSE")
            {
                command c2{ "",parse_arguments(c.params[2].substr(cmd.size() + 1)) };
                process_AC_COMPILE_IFELSE(c2);
            }
            else
            {
                std::cerr << "Unhandled AC_ statement: " << cmd << "\n";
                return nullptr;
            }
        }
        else
        {
            // if key-value?
            if (std::regex_match(c.params[2], r_kv))
            {
                // already handled in 'if (c.params.size() > 1)' above
            }
            else
                return nullptr;
        }
    }

    if (var.empty() || input.empty())
        return nullptr;

    auto p = checks.add<T>(var, input);
    p->DefineIfZero = invert;
    return p.get();
}

template <class T>
T *ac_processor::try_add(command &c)
{
    String var;
    String input = c.params[0];

    input = c.params[0] + "\n\n int main() { \n\n";
    input += c.params[1];
    input += "\n\n ; return 0; }";

    if (c.params.size() > 2)
    {
        if (c.params[2].find("AC_") == 0)
        {
            auto cmd = c.params[2].substr(0, c.params[2].find('('));
            if (cmd == "AC_MSG_RESULT")
                ; // this is a printer
            else if (cmd == "AC_MSG_ERROR")
                ; // this is a printer
            else if (cmd == "AC_MSG_FAILURE")
                ; // this is a printer
            else if (cmd == "AC_DEFINE")
            {
                auto params = parse_arguments(c.params[2].substr(cmd.size() + 1));
                var = params[0];
            }
            else
            {
                std::cerr << "Unhandled AC_ statement: " << cmd << "\n";
                return nullptr;
            }
        }
    }

    if (var.empty() || input.empty())
        return nullptr;

    auto p = checks.add<T>(var, input);
    return p.get();
}

void ac_processor::process_AC_LANG(command &c)
{
    boost::to_lower(c.params[0]);
    cpp = c.params[0] == "c++";
}

void ac_processor::process_AC_DEFINE(command &c)
{
    LOG_ERROR(logger, "process_AC_DEFINE: unimplemented: " + c.params[0]);
    //root["options"]["any"]["definitions"]["public"].push_back(c.params[0]);
}

void ac_processor::process_AC_CHECK_FUNCS(command &c)
{
    split_and_add<sw::FunctionExists>(c);
}

void ac_processor::process_AC_CHECK_DECLS(command &c)
{
    // TODO: add case when in 4th param there's include files
    split_and_add<sw::DeclarationExists>(c);
}

void ac_processor::process_AC_COMPILE_IFELSE(command &c)
{
    auto p = ifdef_add<sw::SourceCompiles>(c);
    if (cpp)
        p->setCpp();
}

void ac_processor::process_AC_RUN_IFELSE(command &c)
{
    auto p = ifdef_add<sw::SourceRuns>(c);
    if (cpp)
        p->setCpp();
}

void ac_processor::process_AC_TRY_COMPILE(command &c)
{
    // sometimes parser swallows empty first arg, so fix it
    if (c.params[1].find("AC_") == 0)
    {
        auto p = c.params;
        c.params.clear();
        c.params.push_back("");
        c.params.insert(c.params.end(), p.begin(), p.end());
    }

    auto p = try_add<sw::SourceCompiles>(c);
    if (cpp)
        p->setCpp();
}

void ac_processor::process_AC_TRY_LINK(command &c)
{
    // sometimes parser swallows empty first arg, so fix it
    if (c.params[1].find("AC_") == 0)
    {
        auto p = c.params;
        c.params.clear();
        c.params.push_back("");
        c.params.insert(c.params.end(), p.begin(), p.end());
    }

    auto p = try_add<sw::SourceLinks>(c);
    if (cpp)
        p->setCpp();
}

void ac_processor::process_AC_TRY_RUN(command &c)
{
    // sometimes parser swallows empty first arg, so fix it
    if (c.params[1].find("AC_") == 0)
    {
        auto p = c.params;
        c.params.clear();
        c.params.push_back("");
        c.params.insert(c.params.end(), p.begin(), p.end());
    }

    auto p = try_add<sw::SourceRuns>(c);
    if (cpp)
        p->setCpp();
}

void ac_processor::process_AC_CHECK_HEADER(command &c)
{
    if (c.params.size() == 1)
    {
        auto out = split_and_add<sw::IncludeExists>(c);
        if (cpp)
        {
            for (auto &o : out)
            {
                if (cpp)
                    o->setCpp();
            }
        }
    }
    else
    {
        if (c.params[1].find("AC_") == 0)
        {
            auto cmd = c.params[1].substr(0, c.params[1].find('('));
            if (cmd == "AC_MSG_RESULT")
                ; // this is a printer
            else if (cmd == "AC_MSG_ERROR")
                ; // this is a printer
            else if (cmd == "AC_MSG_FAILURE")
                ; // this is a printer
            else if (cmd == "AC_DEFINE")
            {
                auto params = parse_arguments(c.params[1].substr(cmd.size() + 1));
                auto p = checks.add<sw::IncludeExists>(c.params[0], params[0]);
                if (cpp)
                    p->setCpp();
            }
            else if (cmd == "AC_CHECK_HEADER")
            {
                auto p = checks.add<sw::IncludeExists>(c.params[0]);
                if (cpp)
                    p->setCpp();

                command c2;
                c2.name = cmd;
                c2.params = parse_arguments(c.params[1].substr(cmd.size() + 1));
                process_AC_CHECK_HEADER(c2);
            }
            else
            {
                std::cerr << "Unhandled AC_ statement: " << cmd << "\n";
            }
        }
        else
        {
            auto p = checks.add<sw::IncludeExists>(c.params[0]);
            if (cpp)
                p->setCpp();
        }
    }
}

void ac_processor::process_AC_CHECK_HEADERS(command &c)
{
    split_and_add<sw::IncludeExists>(c);
}

void ac_processor::process_AC_CHECK_TYPES(command &c)
{
    split_and_add<sw::TypeSize>(c, [](const auto &v)
    {
        if (v == "*" || v == "void")
            return false;
        return true;
    });
}

void ac_processor::process_AC_HEADER_DIRENT(command &)
{
    command c{ "", {"dirent.h","sys/ndir.h","sys/dir.h","ndir.h"} };
    process_AC_CHECK_HEADERS(c);
}

void ac_processor::process_AC_STRUCT_DIRENT_D_TYPE(command &)
{
    command c{ "", {"struct dirent.d_type"} };
    process_AC_HEADER_DIRENT(c);
    process_AC_CHECK_MEMBERS(c);
}

void ac_processor::process_AC_HEADER_ASSERT(command &)
{
    checks.add<sw::IncludeExists>("assert.h");
}

void ac_processor::process_AC_HEADER_SYS_WAIT(command &)
{
    checks.add<sw::IncludeExists>("sys/wait.h");
}

void ac_processor::process_AC_HEADER_STDBOOL(command &c)
{
    checks.add<sw::IncludeExists>("stdbool.h");
}

void ac_processor::process_AC_HEADER_TIME(command &)
{
    command c{ "",{ "time.h","sys/time.h" } };
    process_AC_CHECK_HEADERS(c);
    checks.add<sw::SourceCompiles>("HAVE_TIME_WITH_SYS_TIME", R"(
#include <time.h>
#include <sys/time.h>
int main() {return 0;}
)");
}

void ac_processor::process_AC_HEADER_STDC(command &)
{
    command c{ "",{ "stdlib.h", "stdarg.h", "string.h", "float.h" } };
    process_AC_CHECK_HEADERS(c);
    checks.add<sw::SourceCompiles>("STDC_HEADERS", R"(
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <float.h>
int main() {return 0;}
)");
}

void ac_processor::process_AC_HEADER_MAJOR(command &c)
{
    checks.add<sw::SourceCompiles>("MAJOR_IN_MKDEV", R"(
#include <sys/mkdev.h>
int main() { makedev(0, 0); return 0; }
)");

    checks.add<sw::SourceCompiles>("MAJOR_IN_SYSMACROS", R"(
#include <sys/sysmacros.h>
int main() { makedev(0, 0); return 0; }
)");
}

void ac_processor::process_AC_STRUCT_TM(command &c)
{
    auto p = checks.add<sw::SourceCompiles>("TM_IN_SYS_TIME", R"(
#include <time.h>
int main() { struct tm t; return 0; }
)");
    p->DefineIfZero = true;
}

void ac_processor::process_AC_STRUCT_TIMEZONE(command &c)
{
    // Figure out how to get the current timezone.If struct tm has a tm_zone member,
    // define HAVE_STRUCT_TM_TM_ZONE(and the obsoleted HAVE_TM_ZONE).
    // Otherwise, if the external array tzname is found, define HAVE_TZNAME.
    auto c2 = checks.add<sw::SymbolExists>("tzname");
    c2->Parameters.Includes = { "time.h" };
}

void ac_processor::process_AC_CHECK_LIB(command &c)
{
    checks.add<sw::LibraryFunctionExists>(c.params[1], c.params[0]);
}

void ac_processor::process_AC_CHECK_LIBM(command &c)
{
    // check sin function for example?
    checks.add<sw::LibraryFunctionExists>("m", "sin");
}

void ac_processor::process_AC_CHECK_MEMBERS(command &c)
{
    auto vars = split_string(c.params[0], ",;");
    for (const auto &variable : vars)
    {
        auto p = variable.find('.');
        auto struct_ = variable.substr(0, p);
        auto member = variable.substr(p + 1);
        String header;
        if (struct_ == "struct stat")
            header = "sys/stat.h";
        else if (struct_ == "struct tm")
            header = "time.h";
        else if (struct_ == "struct dirent")
            header = "dirent.h";
        // add more headers here

        auto c2 = checks.add<sw::StructMemberExists>(member, struct_);
        if (!header.empty())
            c2->Parameters.Includes.push_back(header);
    }
}

void ac_processor::process_AC_CHECK_ALIGNOF(command &c)
{
    checks.add<sw::TypeAlignment>(c.params[0]);
}

void ac_processor::process_AC_CHECK_SYMBOL(command &c)
{
    auto c2 = checks.add<sw::SymbolExists>(c.params[0]);
    c2->Parameters.Includes = { c.params[1] };
}
