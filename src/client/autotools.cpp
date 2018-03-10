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

#include "autotools.h"

#include "checks_detail.h"

#include <boost/algorithm/string.hpp>

#include <iostream>
#include <regex>
#include <vector>

struct command
{
    String name;
    Strings params;
};

struct if_expr
{
    struct if_action
    {
        String var;
        bool equ;
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
    Checks checks;
    std::map<String, std::set<value>> vars;
    std::map<String, if_expr> conditions;
    yaml root;
    bool cpp = false;

    ac_processor(const path &p);

    template <class T>
    auto split_and_add(command &c, std::function<bool(String)> fun = std::function<bool(String)>());
    template <class T>
    void ifdef_add(command &c);
    template <class T>
    void try_add(command &c);

    void output();
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

auto parse_arguments(const String &f)
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

auto parse_command(const String &f)
{
    auto i = get_end_of_string_block(f.c_str());
    auto s = f.substr(1, i - 2);
    boost::trim(s);
    return parse_arguments(s);
}

auto parse_configure_ac(String f)
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

auto parse_conditions(String f)
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
        a.equ = sign == "=";
        a.value = val;
        a.start = (int)(m[0].first - s.begin());
        if (sign != "=" && sign != "!=") // TODO: implement '-gt' sign
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

void process_configure_ac(const path &p)
{
    ac_processor proc(p);
    proc.process();
    proc.output();
}

ac_processor::ac_processor(const path &p)
{
    file = read_file(p);

    std::regex dnl("dnl.*?\\n");
    file = std::regex_replace(file, dnl, "\n");

    commands = parse_configure_ac(file);
    conditions = parse_conditions(file);
}

void ac_processor::output()
{
    checks.save(root);
    std::cout << dump_yaml_config(root);
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
        SILENCE(AC_TRY_COMMAND);

        // specific checks
        {
            std::regex r("AC_FUNC_(\\w+)");
            std::smatch m;
            if (std::regex_match(c.name, m, r))
            {
                auto v = m[1].str();
                boost::to_lower(v);
                checks.addCheck<CheckFunction>(v);
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
                checks.addCheck<CheckType>(v);
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
                CheckParameters p;
                p.headers = { "stdio.h" };
                checks.addCheck<CheckSymbol>(f, p);
                continue;
            }
            if constexpr (std::is_same_v<T, CheckType>)
                prepare_type(f);
            out.push_back(checks.addCheck<T>(f));
        }
    }
    return out;
}

template <class T>
void ac_processor::ifdef_add(command &c)
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
            return;
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
            else if (cmd == "AC_DEFINE")
            {
                auto params = parse_arguments(c.params[1].substr(cmd.size() + 1));
                var = params[0];
            }
            else
            {
                std::cerr << "Unhandled AC_ statement: " << cmd << "\n";
                return;
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
                            invert = !act.equ;
                        else
                            invert = act.equ;
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
                            invert = act.equ;
                        else
                            invert = !act.equ;
                    }
                }
            }
            else
                return;
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
                return;
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
                return;
        }
    }

    if (var.empty() || input.empty())
        return;

    auto p = checks.addCheck<T>(var, input);
    p->invert = invert;
}

template <class T>
void ac_processor::try_add(command &c)
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
            else if (cmd == "AC_DEFINE")
            {
                auto params = parse_arguments(c.params[2].substr(cmd.size() + 1));
                var = params[0];
            }
            else
            {
                std::cerr << "Unhandled AC_ statement: " << cmd << "\n";
                return;
            }
        }
    }

    if (var.empty() || input.empty())
        return;

    /*auto p = */checks.addCheck<T>(var, input);
}

void ac_processor::process_AC_LANG(command &c)
{
    boost::to_lower(c.params[0]);
    cpp = c.params[0] == "c++";
}

void ac_processor::process_AC_DEFINE(command &c)
{
    root["options"]["any"]["definitions"]["public"].push_back(c.params[0]);
}

void ac_processor::process_AC_CHECK_FUNCS(command &c)
{
    split_and_add<CheckFunction>(c);
}

void ac_processor::process_AC_CHECK_DECLS(command &c)
{
    // TODO: add case when in 4th param there's include files
    split_and_add<CheckDecl>(c);
}

void ac_processor::process_AC_COMPILE_IFELSE(command &c)
{
    if (cpp)
        ifdef_add<CheckCXXSourceCompiles>(c);
    else
        ifdef_add<CheckCSourceCompiles>(c);
}

void ac_processor::process_AC_RUN_IFELSE(command &c)
{
    if (cpp)
        ifdef_add<CheckCXXSourceRuns>(c);
    else
        ifdef_add<CheckCSourceRuns>(c);
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

    if (cpp)
        try_add<CheckCXXSourceCompiles>(c);
    else
        try_add<CheckCSourceCompiles>(c);
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

    if (cpp)
        try_add<CheckCXXSourceCompiles>(c);
    else
        try_add<CheckCSourceCompiles>(c);
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

    if (cpp)
        try_add<CheckCXXSourceRuns>(c);
    else
        try_add<CheckCSourceRuns>(c);
}

void ac_processor::process_AC_CHECK_HEADER(command &c)
{
    if (c.params.size() == 1)
    {
        auto out = split_and_add<CheckInclude>(c);
        if (cpp)
        {
            for (auto &o : out)
                o->set_cpp(cpp);
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
            else if (cmd == "AC_DEFINE")
            {
                auto params = parse_arguments(c.params[1].substr(cmd.size() + 1));
                auto p = checks.addCheck<CheckInclude>(c.params[0], params[0]);
                if (cpp)
                    p->set_cpp(cpp);
            }
            else if (cmd == "AC_CHECK_HEADER")
            {
                auto p = checks.addCheck<CheckInclude>(c.params[0]);
                if (cpp)
                    p->set_cpp(cpp);

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
            auto p = checks.addCheck<CheckInclude>(c.params[0]);
            if (cpp)
                p->set_cpp(cpp);
        }
    }
}

void ac_processor::process_AC_CHECK_HEADERS(command &c)
{
    split_and_add<CheckInclude>(c);
}

void ac_processor::process_AC_CHECK_TYPES(command &c)
{
    split_and_add<CheckType>(c, [](const auto &v)
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
    checks.addCheck<CheckInclude>("assert.h");
}

void ac_processor::process_AC_HEADER_SYS_WAIT(command &)
{
    checks.addCheck<CheckInclude>("sys/wait.h");
}

void ac_processor::process_AC_HEADER_STDBOOL(command &c)
{
    checks.addCheck<CheckInclude>("stdbool.h");
}

void ac_processor::process_AC_HEADER_TIME(command &)
{
    command c{ "",{ "time.h","sys/time.h" } };
    process_AC_CHECK_HEADERS(c);
    checks.addCheck<CheckCSourceCompiles>("HAVE_TIME_WITH_SYS_TIME", R"(
#include <time.h>
#include <sys/time.h>
int main() {return 0;}
)");
}

void ac_processor::process_AC_HEADER_STDC(command &)
{
    command c{ "",{ "stdlib.h", "stdarg.h", "string.h", "float.h" } };
    process_AC_CHECK_HEADERS(c);
    checks.addCheck<CheckCSourceCompiles>("STDC_HEADERS", R"(
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <float.h>
int main() {return 0;}
)");
}

void ac_processor::process_AC_HEADER_MAJOR(command &c)
{
    checks.addCheck<CheckCSourceCompiles>("MAJOR_IN_MKDEV", R"(
#include <sys/mkdev.h>
int main() { makedev(0, 0); return 0; }
)");

    checks.addCheck<CheckCSourceCompiles>("MAJOR_IN_SYSMACROS", R"(
#include <sys/sysmacros.h>
int main() { makedev(0, 0); return 0; }
)");
}

void ac_processor::process_AC_STRUCT_TM(command &c)
{
    auto p = checks.addCheck<CheckCSourceCompiles>("TM_IN_SYS_TIME", R"(
#include <time.h>
int main() { struct tm t; return 0; }
)");
    p->invert = true;
}

void ac_processor::process_AC_STRUCT_TIMEZONE(command &c)
{
    // Figure out how to get the current timezone.If struct tm has a tm_zone member,
    // define HAVE_STRUCT_TM_TM_ZONE(and the obsoleted HAVE_TM_ZONE).
    // Otherwise, if the external array tzname is found, define HAVE_TZNAME.
    CheckParameters p;
    p.headers = { "time.h" };
    checks.addCheck<CheckSymbol>("tzname", p);
}

void ac_processor::process_AC_CHECK_LIB(command &c)
{
    checks.addCheck<CheckLibraryFunction>(c.params[1], c.params[0]);
}

void ac_processor::process_AC_CHECK_LIBM(command &c)
{
    checks.addCheck<CheckLibrary>("m");
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

        CheckParameters pa;
        if (!header.empty())
            pa.headers.push_back(header);
        checks.addCheck<CheckStructMember>(member, struct_, pa);
    }
}

void ac_processor::process_AC_CHECK_ALIGNOF(command &c)
{
    checks.addCheck<CheckAlignment>(c.params[0]);
}

void ac_processor::process_AC_CHECK_SYMBOL(command &c)
{
    CheckParameters p;
    p.headers = { c.params[1] };
    checks.addCheck<CheckSymbol>(c.params[0], p);
}
