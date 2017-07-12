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

#include "fix_imports.h"

#include <config.h>
#include <context.h>
#include <project_path.h>

#include <printers/cmake.h>

#include <boost/algorithm/string.hpp>
#include <boost/nowide/fstream.hpp>

#include <deque>
#include <iostream>
#include <regex>
#include <vector>

String fix_imports(const Strings &lines_old, const String &old_target, const String &new_target)
{
    CMakeContext ctx;
    ctx.increaseIndent();
    for (auto &line1 : lines_old)
    {
        auto line = line1;
        boost::algorithm::trim(line);
        line = line.substr(0, line.find(old_target)) + new_target + line.substr(line.find(old_target) + old_target.size());
        if (line.find("add_library") == 0 || line.find("add_executable") == 0)
            boost::algorithm::replace_all(line, "IMPORTED", "IMPORTED GLOBAL");
        ctx.addLine(line);
    }
    ctx.decreaseIndent();
    ctx.before().addLine("if (NOT TARGET " + new_target + ")");
    ctx.after().addLine("endif()");
    ctx.after().emptyLines(3);
    ctx.splitLines();
    return ctx.getText();
}

void fix_imports(const String &target, const path &aliases_file, const path &old_file, const path &new_file)
{
    auto s = read_file(old_file);
    auto aliases_s = read_file(aliases_file);
    auto dep = extractFromString(target);

    if (!new_file.parent_path().empty())
        fs::create_directories(new_file.parent_path());
    boost::nowide::ofstream ofile(new_file.string());
    if (!ofile)
        throw std::runtime_error("Cannot open the output file for writing");

    // finds all inside round brackets ()
    // also checks that closing bracket ) is not in quotes
    String add_library = "(add_library|add_executable|set_property|set_target_properties)\\(";

    Strings lines;
    std::regex r(add_library);
    std::smatch m;
    bool exe = false;
    while (std::regex_search(s, m, r))
    {
        exe |= m[1].str() == "add_executable";

        auto b = m[1].first - s.begin();
        auto e = get_end_of_string_block(s, (int)(m.suffix().first - s.begin()));

        lines.push_back(s.substr(b, e - b));
        s = m.suffix();
    }

    // set exe imports only to release binary
    // maybe add an option for this behavior later
    auto lines_not_exe = lines;
    if (exe)
    {
        String rel_conf = "IMPORTED_LOCATION_RELEASE";
        String confs = "(IMPORTED_LOCATION_DEBUG|IMPORTED_LOCATION_MINSIZEREL|IMPORTED_LOCATION_RELWITHDEBINFO)";
        String rpath = "\\s*(\".*?\")";
        String iloc = confs + rpath;
        String release_path;
        for (auto &line : lines)
        {
            if (line.find(rel_conf) != line.npos)
            {
                r = rel_conf + rpath;
                if (std::regex_search(line, m, r))
                {
                    release_path = m[1].str();
                    r = iloc;
                    for (auto &line2 : lines)
                    {
                        if (std::regex_search(line2, m, r))
                        {
                            String t;
                            t = m.prefix().str();
                            t += m[1].str();
                            t += " ";
                            t += release_path;
                            t += m.suffix().str();
                            line2 = t;
                        }
                    }
                }
                else
                    std::cerr << "Cannot extract file path from IMPORTED_LOCATION_RELEASE\n"; // cannot find std?
                break;
            }
        }
    }

    auto fix = [&aliases_s](const auto &lines, const auto &dep)
    {
        const auto &tgt = dep.target_name_hash;
        CMakeContext ctx;

        StringSet aliases;
        {
            Strings aliasesv;
            boost::algorithm::trim(aliases_s);
            boost::algorithm::split(aliasesv, aliases_s, boost::is_any_of(";"));
            for (auto &a : aliasesv)
                boost::algorithm::trim(a);
            aliases.insert(aliasesv.begin(), aliasesv.end());
        }

        add_aliases(ctx, dep, true, aliases, [&lines, &tgt](const auto &s, const auto &v)
        {
            return fix_imports(lines, tgt, s);
        });

        ctx.emptyLines(1);
        ctx.splitLines();
        return ctx.getText();
    };

    CMakeContext ctx;
    file_header(ctx, dep);
    if (exe)
    {
        ctx.addLine("if (CPPAN_BUILD_EXECUTABLES_WITH_SAME_CONFIGURATION)");
        ctx.increaseIndent();
        ctx.addLine(fix(lines_not_exe, dep));
        ctx.decreaseIndent();
        ctx.addLine("else()");
        ctx.increaseIndent();
        ctx.addLine(fix(lines, dep));
        ctx.decreaseIndent();
        ctx.addLine("endif()");
    }
    else
    {
        ctx.addLine(fix(lines, dep));
    }
    file_footer(ctx, dep);

    ctx.splitLines();
    auto t = ctx.getText();
    boost::replace_all(t, "\r", "");
    ofile << t;
}
