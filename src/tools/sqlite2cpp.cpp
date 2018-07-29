/*
 * Copyright (C) 2018 Egor Pugin
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

#include <boost/algorithm/string.hpp>
#include <primitives/context.h>
#include <primitives/filesystem.h>
#include <primitives/sw/main.h>
#include <primitives/sw/settings.h>
#include <sqlite3.h>

#include <functional>
#include <iostream>
#include <regex>

#define SQLITE_CALLBACK_ARGS int ncols, char** cols, char** names

using DatabaseCallback = std::function<int(int /*ncols*/, char** /*cols*/, char** /*names*/)>;

sqlite3 *db;

String INCLUDE = "sqlpp11";
String NAMESPACE = "sqlpp";

StringMap<String> types{
    {
        "integer",
        "integer",
    },
    {
        "text",
        "text",
    },
    {
        "blob",
        "blob",
    },
    {
        "real",
        "floating_point",
    },
};

String get_type(const String &in)
{
    for (auto &t : types)
    {
        if (in.find(t.first) == 0)
            return t.second;
    }
    return {};
}

void execute(const String &sql, DatabaseCallback callback = 0)
{
    char *errmsg;
    auto cb = [](void *o, int ncols, char **cols, char **names)
    {
        auto f = (DatabaseCallback *)o;
        if (*f)
            return (*f)(ncols, cols, names);
        return 0;
    };
    sqlite3_exec(db, sql.c_str(), cb, &callback, &errmsg);
    if (errmsg)
    {
        auto s = sql.substr(0, 200);
        if (sql.size() > 200)
            s += "...";
        std::cerr << "Error executing sql statement:\n" << s << "\nError: " << errmsg;
        exit(1);
    }
}

auto repl_camel_case_func(const std::smatch &m, String &o)
{
    auto s = m[2].str();
    s[0] = toupper(s[0]);
    if (m[1] == "_")
        return s;
    else
        return m[1].str() + s;
}

auto toName(String s, const String &rgx)
{
    String o;
    std::regex r(rgx);
    std::smatch m;
    while (std::regex_search(s, m, r))
    {
        o += m.prefix().str();
        o += repl_camel_case_func(m, o);
        s = m.suffix().str();
    }
    o += s;
    return o;
}

auto toClassName(String s)
{
    s[0] = toupper(s[0]);
    return toName(s, "(\\s|[_0-9])(\\S)");
}

auto toMemberName(String s)
{
    return toName(s, "(\\s|_|[0-9])(\\S)");
}

auto escape_if_reserved(const String &name)
{
    StringSet reserved_names{
        "GROUP",
        "ORDER",
    };
    if (reserved_names.count(boost::to_upper_copy(name)))
        return "\"" + name + "\"";
    return name;
}

int main(int argc, char **argv)
{
    cl::opt<path> ddl(cl::Positional, cl::desc("<input sql script>"), cl::Required);
    cl::opt<path> target(cl::Positional, cl::desc("<output .cpp file>"), cl::Required);
    cl::opt<std::string> ns(cl::Positional, cl::desc("<namespace>"), cl::Required);

    cl::parseCommandLineOptions(argc, argv);

    sqlite3_open(":memory:", &db);
    execute(read_file(ddl).c_str());

    Context ctx;

    // start printing
    ctx.addLine("// generated file, do not edit");
    ctx.addLine();
    ctx.addLine("#pragma once");
    ctx.addLine();
    ctx.addLine("#include <" + INCLUDE + "/table.h>");
    ctx.addLine("#include <" + INCLUDE + "/data_types.h>");
    ctx.addLine("#include <" + INCLUDE + "/char_sequence.h>");
    ctx.addLine();
    ctx.beginNamespace(ns);

    Strings tables;
    execute("SELECT name FROM sqlite_master WHERE type=\"table\"", [&tables](SQLITE_CALLBACK_ARGS)
    {
        String table_name = cols[0];
        if (table_name.find("sqlite_") == 0)
            return 0;
        tables.push_back(table_name);
        return 0;
    });

    for (auto &table_name : tables)
    {
        auto sqlTableName = table_name;
        auto tableClass = toClassName(sqlTableName);
        auto tableMember = toMemberName(sqlTableName);
        auto tableNamespace = tableClass + "_";
        auto tableTemplateParameters = tableClass;
        ctx.beginNamespace(tableNamespace);

        execute("PRAGMA table_info(" + table_name + ")", [&](SQLITE_CALLBACK_ARGS)
        {
            String sqlColumnName = cols[1];
            auto columnClass = toClassName(sqlColumnName);
            tableTemplateParameters += ",\n               " + tableNamespace + "::" + columnClass;
            auto columnMember = toMemberName(sqlColumnName);
            String sqlColumnType = cols[2];
            sqlColumnType = boost::to_lower_copy(sqlColumnType);
            ctx.beginBlock("struct " + columnClass);
            ctx.beginBlock("struct _alias_t");
            ctx.addLine("static constexpr const char _literal[] = \"" + escape_if_reserved(sqlColumnName) + "\";");
            ctx.emptyLines();
            ctx.addLine("using _name_t = sqlpp::make_char_sequence<sizeof(_literal), _literal>;");
            ctx.emptyLines();
            ctx.addLine("template<typename T>");
            ctx.beginBlock("struct _member_t");
            ctx.addLine("T " + columnMember + ";");
            ctx.emptyLines();
            ctx.addLine("T& operator()() { return " + columnMember + "; }");
            ctx.addLine("const T& operator()() const { return " + columnMember + "; }");
            ctx.endBlock(true);
            ctx.endBlock(true);
            ctx.emptyLines();

            auto t = get_type(sqlColumnType);
            if (t.empty())
            {
                std::cerr << "Error: datatype " + sqlColumnType + " is not supported.";
                exit(1);
            }

            Strings traitslist;
            traitslist.push_back(NAMESPACE + "::" + t);

            auto requireInsert = true;
            auto hasAutoValue = sqlColumnName == "id";
            if (hasAutoValue)
            {
                traitslist.push_back(NAMESPACE + "::tag::must_not_insert");
                traitslist.push_back(NAMESPACE + "::tag::must_not_update");
                requireInsert = false;
            }
            if (String(cols[3]) == "0")
            {
                traitslist.push_back(NAMESPACE + "::tag::can_be_null");
                requireInsert = false;
            }
            if (cols[4])
                requireInsert = false;
            if (requireInsert)
                traitslist.push_back(NAMESPACE + "::tag::require_insert");
            String l;
            for (auto &li : traitslist)
                l += li + ", ";
            if (!l.empty())
                l.resize(l.size() - 2);
            ctx.addLine("using _traits = " + NAMESPACE + "::make_traits<" + l + ">;");
            ctx.endBlock(true);
            ctx.emptyLines();
            return 0;
        });

        ctx.endNamespace(tableNamespace);
        ctx.emptyLines();

        ctx.beginBlock("struct " + tableClass + ": " + NAMESPACE + "::table_t<" + tableTemplateParameters + ">");
        ctx.beginBlock("struct _alias_t");
        ctx.addLine("static constexpr const char _literal[] = \"" + sqlTableName + "\";");
        ctx.emptyLines();
        ctx.addLine("using _name_t = sqlpp::make_char_sequence<sizeof(_literal), _literal>;");
        ctx.emptyLines();
        ctx.addLine("template<typename T>");
        ctx.beginBlock("struct _member_t");
        ctx.addLine("T " + tableMember + ";");
        ctx.emptyLines();
        ctx.addLine("T& operator()() { return " + tableMember + "; }");
        ctx.addLine("const T& operator()() const { return " + tableMember + "; }");
        ctx.endBlock(true);
        ctx.endBlock(true);
        ctx.endBlock(true);
        ctx.emptyLines();
    }

    ctx.endNamespace(ns);

    write_file(target, ctx.getText());

    return 0;
}
