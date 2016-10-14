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

extern const std::map<int, Check::Information> check_information;

Check::Information getCheckInformation(int type);

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
        variable = convert(data);
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

    void set_cpp(bool c)
    {
        cpp = c;
        if (cpp)
            information.function = "CHECK_INCLUDE_FILE_CXX";
        else
            information.function = getCheckInformation(Include).function;
    }

    static String convert(const String &s)
    {
        auto v_def = "HAVE_" + boost::algorithm::to_upper_copy(s);
        for (auto &c : v_def)
        {
            if (!isalnum(c))
                c = '_';
        }
        return v_def;
    }

    virtual ~CheckInclude() {}

private:
    bool cpp = false;
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

    void save(yaml &root) const override
    {
        for (auto &h : headers)
            root[information.cppan_key][getData()].push_back(h);
    }

private:
    std::set<String> headers;
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
    auto p = i.get();
    checks.emplace(std::move(i));
    return p;
}
