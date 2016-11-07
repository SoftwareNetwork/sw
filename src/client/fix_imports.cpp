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

#include "fix_imports.h"

#include <common.h>
#include <config.h>
#include <context.h>
#include <project_path.h>

#include <boost/algorithm/string.hpp>

#include <deque>
#include <iostream>
#include <regex>
#include <vector>

using Lines = std::vector<String>;

String fix_imports(const Lines &lines_old, const String &old_target, const String new_target)
{
    Context ctx;
    ctx.increaseIndent();
    for (auto &line1 : lines_old)
    {
        auto line = line1;
        boost::algorithm::trim(line);
        line = line.substr(0, line.find(old_target)) + new_target + line.substr(line.find(old_target) + old_target.size());
        if (line.find("add_library") == 0 || line.find("add_executable") == 0)
            boost::algorithm::replace_all(line, "IMPORTED", "IMPORTED GLOBAL");
        else if (line.find("set_target_properties") == 0)
        {
            static std::regex r("INTERFACE_LINK_LIBRARIES\\s*\\S+");
            line = std::regex_replace(line, r, "");
        }
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

    std::ofstream ofile(new_file.string());
    if (!ofile)
        throw std::runtime_error("Cannot open the output file for writing");

    if (dep.version.isBranch())
    {
        boost::algorithm::replace_all(s, "\r", "");
        ofile << s;
        return;
    }

    // finds all inside round brackets ()
    // also checks that closing bracket ) is not in quotes
    String basic = R"(\([^\)]*?\))";
    String add_library = "(add_library|add_executable|set_property|set_target_properties)" + basic;

    Lines lines;
    std::regex r(add_library);
    std::smatch m;
    bool exe = false;
    while (std::regex_search(s, m, r))
    {
        exe |= m[1].str() == "add_executable";
        lines.push_back(m.str());
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

    auto fix = [&target, &aliases_s](const auto &lines, auto dep)
    {
        Context ctx;
        {
            auto d = dep;
            // add GLOBAL for default target
            ctx.addLine(fix_imports(lines, target, d.ppath.toString() + "-" + d.version.toAnyVersion()));
            d.version.patch = -1;
            ctx.addLine(fix_imports(lines, target, d.ppath.toString() + "-" + d.version.toAnyVersion()));
            d.version.minor = -1;
            ctx.addLine(fix_imports(lines, target, d.ppath.toString() + "-" + d.version.toAnyVersion()));
            ctx.addLine(fix_imports(lines, target, d.ppath.toString()));
        }
        {
            auto d = dep;
            ctx.addLine(fix_imports(lines, target, d.ppath.toString("::") + "-" + d.version.toAnyVersion()));
            d.version.patch = -1;
            ctx.addLine(fix_imports(lines, target, d.ppath.toString("::") + "-" + d.version.toAnyVersion()));
            d.version.minor = -1;
            ctx.addLine(fix_imports(lines, target, d.ppath.toString("::") + "-" + d.version.toAnyVersion()));
            ctx.addLine(fix_imports(lines, target, d.ppath.toString("::")));
        }
        {
            Lines aliases;
            boost::algorithm::trim(aliases_s);
            boost::algorithm::split(aliases, aliases_s, boost::is_any_of(";"));
            for (auto &a : aliases)
                boost::algorithm::trim(a);
            for (auto &a : aliases)
                if (!a.empty())
                    ctx.addLine(fix_imports(lines, target, a));
        }
        ctx.emptyLines(1);
        ctx.splitLines();
        return ctx.getText();
    };

    Context ctx;
    if (exe)
    {
        ctx.addLine("if (CPPAN_BUILD_EXECUTABLES_WITH_SAME_CONFIG)");
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

    ctx.splitLines();
    auto t = ctx.getText();
    boost::replace_all(t, "\r", "");
    ofile << t;
}
