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

#include "bazel.h"

#include "driver.h"
#include "../common/yaml.h"

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

Values File::getFiles(const Name &name, const std::string &bazel_target_function)
{
    Values values;
    for (auto &f : functions)
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
    return values;
}

File parse(const std::string &s)
{
    BazelParserDriver pd;
    pd.parse(s);
    pd.bazel_file.trimQuotes();
    return pd.bazel_file;
}

} // namespace bazel

void process_bazel(const path &p, const std::string &libname = "cc_library", const std::string &binname = "cc_binary")
{
    auto prepare_dep_name = [](auto s)
    {
        //if (s.find("//") == 0)
            //return std::string();
        prepare_project_name(s);
        boost::replace_all(s, ":", "");
        boost::replace_all(s, "+", "p");
        return s;
    };

    auto b = read_file(p);
    auto file = bazel::parse(b);
    yaml root;
    auto projects = root["projects"];
    for (auto &f : file.functions)
    {
        enum
        {
            unk,
            lib,
            bin,
        };
        int type = unk;
        if (pystring::endswith(f.name, libname))
            type = lib;
        else if (pystring::endswith(f.name, binname))
            type = bin;
        else
            continue;

        auto i = std::find_if(f.parameters.begin(), f.parameters.end(), [](const auto &p)
        {
            return "name" == p.name;
        });
        if (i == f.parameters.end() || i->values.empty())
            continue;

        auto pname = prepare_project_name(*i->values.begin());
        auto project = projects[pname];
        if (type == lib)
            project["type"] = "lib";

        project["import_from_bazel"] = true;
        project["bazel_target_name"] = *i->values.begin();
        project["bazel_target_function"] = type == lib ? libname : binname;

        for (auto &n : { "deps", "external_deps" })
        {
            i = std::find_if(f.parameters.begin(), f.parameters.end(), [&n](const auto &p)
            {
                return n == p.name;
            });
            if (!(i == f.parameters.end() || i->values.empty()))
            {
                for (auto &d : i->values)
                {
                    auto d2 = prepare_dep_name(d);
                    if (!d2.empty())
                        project["dependencies"].push_back(d2);
                }
            }
        }
    }

    std::cout << dump_yaml_config(root);
}
