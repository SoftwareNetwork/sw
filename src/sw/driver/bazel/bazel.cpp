// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2016-2020 Egor Pugin <egor.pugin@gmail.com>

#include "bazel.h"

#include "driver.h"

#include <boost/algorithm/string.hpp>
#include <primitives/filesystem.h>
#include <pystring.h>

#include <algorithm>
#include <regex>

namespace {

void trimQuotes(std::string &s)
{
    if (s.empty())
        return;
    if (s.front() == '\"')
        s = s.substr(1);
    if (s.empty())
        return;
    if (s.back() == '\"')
        s = s.substr(0, s.size() - 1);

    //s = s.substr(s.find_first_not_of("\""));
    //s = s.substr(0, s.find_last_not_of("\"") + 1);
}

std::string prepare_project_name(const std::string &s)
{
    std::string t = s;
    std::replace(t.begin(), t.end(), '-', '_');
    std::replace(t.begin(), t.end(), '+', 'p');
    return t;
}

}

namespace bazel
{

void Parameter::trimQuotes()
{
    ::trimQuotes(name);
    Values vls;
    for (auto &v : values)
    {
        auto s = v;
        ::trimQuotes(s);
        vls.insert(s);
    }
    values = vls;
}

void Function::trimQuotes()
{
    ::trimQuotes(name);
    for (auto &p : parameters)
        p.trimQuotes();
}

void File::trimQuotes()
{
    for (auto &f : functions)
        f.trimQuotes();
}

Values File::getFiles(const Name &name, const std::string &bazel_target_function) const
{
    Values values;
    for (const auto &f : functions)
    {
        if (!(
            pystring::endswith(f.name, "cc_library") ||
            pystring::endswith(f.name, "cc_binary") ||
            pystring::endswith(f.name, bazel_target_function)
            ))
            continue;

        auto i = std::find_if(f.parameters.begin(), f.parameters.end(), [](const auto &p)
        {
            return "name" == p.name;
        });
        if (i == f.parameters.end() || i->values.empty() ||
            (prepare_project_name(*i->values.begin()) != name && *i->values.begin() != name))
            continue;

        for (auto &n : { "hdrs", "public_hdrs" })
        {
            i = std::find_if(f.parameters.begin(), f.parameters.end(), [&n](const auto &p)
            {
                return p.name == n;
            });
            if (i != f.parameters.end())
            {
                // check if we has a variable
                for (auto &v : i->values)
                {
                    auto p = parameters.find(v);
                    if (p != parameters.end())
                        values.insert(p->second.values.begin(), p->second.values.end());
                    else
                        values.insert(i->values.begin(), i->values.end());
                }
            }
        }

        i = std::find_if(f.parameters.begin(), f.parameters.end(), [](const auto &p)
        {
            return "srcs" == p.name;
        });
        if (i != f.parameters.end())
        {
            // check if we has a variable
            for (const auto &v : i->values)
            {
                auto p = parameters.find(v);
                if (p != parameters.end())
                    values.insert(p->second.values.begin(), p->second.values.end());
                else
                    values.insert(i->values.begin(), i->values.end());
            }
        }
    }
    Values vls;
    for (const auto &v : values)
    {
        auto s = v;
        ::trimQuotes(s);
        vls.insert(s);
    }
    return vls;
}

File parse(const std::string &s)
{
    BazelParserDriver pd;
    pd.parse(s);
    pd.bazel_file.trimQuotes();
    return pd.bazel_file;
}

} // namespace bazel
