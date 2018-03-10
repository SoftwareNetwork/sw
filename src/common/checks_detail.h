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

#include "printers/printer.h"

#include <boost/algorithm/string.hpp>
#include <primitives/context.h>

#include <memory>

extern const std::map<int, Check::Information> check_information;

class CheckParametersScopedWriter
{
    CMakeContext &ctx;
    const CheckParameters &p;
    bool with_headers;
public:
    CheckParametersScopedWriter(CMakeContext &ctx, const CheckParameters &p, bool with_headers = false)
        : ctx(ctx), p(p), with_headers(with_headers)
    {
        if (with_headers)
            p.writeHeadersBefore(ctx);
        p.writeBefore(ctx);
    }
    ~CheckParametersScopedWriter()
    {
        p.writeAfter(ctx);
        if (with_headers)
            p.writeHeadersAfter(ctx);
    }
};

class CheckFunction : public Check
{
public:
    CheckFunction(const String &f, const CheckParameters &p = CheckParameters())
        : Check(getCheckInformation(Function), p)
    {
        data = f;
        variable = "HAVE_" + boost::algorithm::to_upper_copy(data);
    }

    virtual ~CheckFunction() {}

    void save(yaml &root) const override
    {
        if (parameters.empty())
        {
            root[information.cppan_key].push_back(getData());
            return;
        }

        yaml y;
        y["function"] = getData();
        parameters.save(y);
        root[information.cppan_key].push_back(y);
    }

    void writeCheck(CMakeContext &ctx) const override
    {
        CheckParametersScopedWriter p(ctx, parameters);
        ctx.addLine(information.function + "(" + getData() + " " + getVariable() + ")");
    }
};

class CheckInclude : public Check
{
public:
    CheckInclude(const String &s)
        : Check(getCheckInformation(Include))
    {
        data = s;
        variable = make_include_var(data);
    }

    CheckInclude(const String &s, const String &var)
        : Check(getCheckInformation(Include))
    {
        data = s;
        variable = var;
    }

    void save(yaml &root) const override
    {
        yaml v;
        v["file"] = getData();
        v["variable"] = getVariable();
        v["cpp"] = cpp;
        root[information.cppan_key].push_back(v);
    }

    void set_cpp(bool c) override
    {
        cpp = c;
        if (cpp)
            information.function = "CHECK_INCLUDE_FILE_CXX";
        else
            information.function = getCheckInformation(Include).function;
    }

    virtual ~CheckInclude() {}
};

class CheckType : public Check
{
public:
    CheckType(const String &t, const String &prefix = "HAVE_")
        : Check(getCheckInformation(Type))
    {
        data = t;
        variable = make_type_var(data, prefix);
    }

    CheckType(const String &t, const CheckParameters &p)
        : Check(getCheckInformation(Type), p)
    {
        data = t;
        variable = make_type_var(data);
    }

    virtual ~CheckType() {}

    void writeCheck(CMakeContext &ctx) const override
    {
        CheckParametersScopedWriter p(ctx, parameters, true);
        ctx.addLine(information.function + "(\"" + getData() + "\" " + getVariable() + ")");
    }

    void save(yaml &root) const override
    {
        yaml n;
        n["type"] = getData();
        parameters.save(n);
        root[information.cppan_key].push_back(n);
    }
};

class CheckStructMember : public Check
{
public:
    CheckStructMember(const String &m, const String &s, const CheckParameters &p = CheckParameters())
        : Check(getCheckInformation(StructMember), p)
    {
        data = m;
        struct_ = s;
        variable = make_struct_member_var(data, struct_);
    }

    virtual ~CheckStructMember() {}

    void writeCheck(CMakeContext &ctx) const override
    {
        CheckParametersScopedWriter p(ctx, parameters);
        ctx.addLine(information.function + "(\"" + struct_ + "\" \"" + getData() + "\" \"");
        for (auto &h : parameters.headers)
            ctx.addText(h + ";");
        ctx.addText("\" " + getVariable());
        if (cpp)
            ctx.addText(" LANGUAGE CXX");
        ctx.addText(")");
    }

    void save(yaml &root) const override
    {
        yaml n;
        n["member"] = getData();
        n["struct"] = struct_;
        parameters.save(n);
        root[information.cppan_key].push_back(n);
    }

    String printStatus() const override
    {
        if (getValue())
            return "-- " + information.singular + " " + getData() + " of " + struct_ + " - found (" + std::to_string(getValue()) + ")";
        else
            return "-- " + information.singular + " " + getData() + " of " + struct_ + " - not found";
    }

public:
    String struct_;
};

class CheckAlignment : public Check
{
public:
    CheckAlignment(const String &s, const String &prefix = "ALIGNOF_")
        : Check(getCheckInformation(Alignment))
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

    virtual ~CheckAlignment() {}
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

class CheckLibraryFunction : public Check
{
public:
    CheckLibraryFunction(const String &s, const String &lib)
        : Check(getCheckInformation(LibraryFunction))
    {
        data = s;
        variable = "HAVE_" + boost::algorithm::to_upper_copy(data);
        library = lib;
    }

    virtual ~CheckLibraryFunction() {}

    void save(yaml &root) const override
    {
        yaml v;
        v["function"] = getData();
        v["library"] = library;
        root[information.cppan_key].push_back(v);
    }

    String library;
};

class CheckSymbol : public Check
{
public:
    CheckSymbol(const String &s, const CheckParameters &p = CheckParameters())
        : Check(getCheckInformation(Symbol), p)
    {
        data = s;
        variable = "HAVE_" + boost::algorithm::to_upper_copy(data);
    }

    virtual ~CheckSymbol() {}

    void writeCheck(CMakeContext &ctx) const override
    {
        CheckParametersScopedWriter p(ctx, parameters);
        ctx.addLine(information.function + "(\"" + getData() + "\" \"");
        for (auto &h : parameters.headers)
            ctx.addText(h + ";");
        ctx.addText("\" " + getVariable() + ")");
    }

    void save(yaml &root) const override
    {
        yaml n;
        if (cpp)
            n["cpp"] = cpp;
        n["symbol"] = getData();
        parameters.save(n);
        root[information.cppan_key].push_back(n);
    }

    void set_cpp(bool c) override
    {
        cpp = c;
        if (cpp)
            information.function = "check_cxx_symbol_exists";
        else
            information.function = getCheckInformation(Symbol).function;
    }
};

class CheckDecl : public Check
{
public:
    CheckDecl(const String &s, const CheckParameters &p = CheckParameters())
        : Check(getCheckInformation(Decl), p)
    {
        data = s;
        variable = "HAVE_DECL_" + boost::algorithm::to_upper_copy(data);
    }

    virtual ~CheckDecl() {}

    void save(yaml &root) const override
    {
        yaml n;
        n["decl"] = getData();
        parameters.save(n);
        root[information.cppan_key].push_back(n);
    }

    void writeCheck(CMakeContext &ctx) const override
    {
        static const Strings headers = {
            "HAVE_SYS_TYPES_H",
            "HAVE_SYS_STAT_H",
            "STDC_HEADERS",
            "HAVE_STDLIB_H",
            "HAVE_STRING_H",
            "HAVE_MEMORY_H",
            "HAVE_STRINGS_H",
            "HAVE_INTTYPES_H",
            "HAVE_STDINT_H",
            "HAVE_UNISTD_H",
        };

        auto print_header_def = [&ctx](const auto &h)
        {
            ctx.addLine("if (" + h + ")");
            ctx.addLine("set(CMAKE_REQUIRED_DEFINITIONS ${CMAKE_REQUIRED_DEFINITIONS} -D" + h + "=${" + h + "})");
            ctx.addLine("endif()");
        };

        ctx.addLine("set(CMAKE_REQUIRED_DEFINITIONS)");
        for (auto &h : headers)
            print_header_def(h);
        String more_headers;
        for (auto &h : parameters.headers)
        {
            auto iv = make_include_var(h);
            print_header_def(iv);
            more_headers += "#ifdef " + iv + "\n";
            more_headers += "# include <" + h + ">\n";
            more_headers += "#endif\n";
        }

        ctx.addLine(information.function + "(\"" +
            R"(

#include <stdio.h>
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifdef STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# ifdef HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif
#ifdef HAVE_STRING_H
# if !defined STDC_HEADERS && defined HAVE_MEMORY_H
#  include <memory.h>
# endif
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#ifdef HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

)" +
more_headers +
R"(

int main()
{
    (void)
)" +
            getData() +
            R"(
    ;
    return 0;
}
)"
            "\" " + getVariable() + ")");

        ctx.addLine("set(CMAKE_REQUIRED_DEFINITIONS)");
    }
};

struct CheckSource : public Check
{
    bool invert = false;

    CheckSource(const Check::Information &i)
        : Check(i)
    {
    }

    virtual ~CheckSource() {}

    void save(yaml &root) const override
    {
        root[information.cppan_key][getVariable()]["text"] = getData();
        root[information.cppan_key][getVariable()]["invert"] = invert;
    }

    String printStatus() const override
    {
        if (isOk())
            return "-- Test " + getVariable() + " - Success (" + std::to_string(getValue()) + ")";
        else
            return "-- Test " + getVariable() + " - Failed";
    }

    bool isOk() const override
    {
        return (!invert && getValue()) || (invert && !getValue());
    }
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

template <class T, class ... Args>
T *Checks::addCheck(Args && ... args)
{
    auto i = std::make_shared<T>(std::forward<Args>(args)...);
    auto r = checks.emplace(std::move(i));
    return (T*)r.first->get();
}
