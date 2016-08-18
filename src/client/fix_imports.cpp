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
#include <cppan.h>
#include <project_path.h>

#include <boost/algorithm/string.hpp>

#include <iostream>
#include <regex>
#include <vector>

using Lines = std::vector<String>;

Lines fix_imports(const Lines &lines_old, const String &old_target, const String new_target)
{
    auto lines = lines_old;
    for (auto &line : lines)
    {
        boost::algorithm::trim(line);
        line = line.substr(0, line.find(old_target)) + new_target + line.substr(line.find(old_target) + old_target.size());
        if (line.find("add_library") == 0 || line.find("add_executable") == 0)
            boost::algorithm::replace_all(line, "IMPORTED", "IMPORTED GLOBAL");
    }
    return lines;
}

void operator+=(String &s, const Lines &lines)
{
    for (auto &line : lines)
        s += line + "\n";
    s += "\n\n\n";
}

void fix_imports(const String &target, const path &aliases_file, const path &old_file, const path &new_file)
{
    auto s = read_file(old_file);
    auto aliases_s = read_file(aliases_file);

    ProjectPath p = target.substr(0, target.find('-'));
    Version v = target.substr(target.find('-') + 1);
    Dependency dep{ p,v };

    std::ofstream ofile(new_file.string());
    if (!ofile)
        throw std::runtime_error("Cannot open the output file for writing");

    if (v.isBranch())
    {
        ofile << s;
        return;
    }

    // finds all inside round brackets ()
    // also checks that closing bracket ) is not in quotes
    String basic = R"r(\([^>]*?(?:(?:('|")[^'"]*?\1)[^>]*?)*\))r";
    String add_library = "(add_library|add_executable|set_property|set_target_properties)" + basic;

    Lines lines;
    std::regex r(add_library);
    std::smatch m;
    while (std::regex_search(s, m, r))
    {
        lines.push_back(m.str());
        s = m.suffix();
    }

    String result;
    {
        auto d = dep;
        // add GLOBAL for default target
        result += fix_imports(lines, target, d.package.toString() + "-" + d.version.toAnyVersion());
        d.version.patch = -1;
        result += fix_imports(lines, target, d.package.toString() + "-" + d.version.toAnyVersion());
        d.version.minor = -1;
        result += fix_imports(lines, target, d.package.toString() + "-" + d.version.toAnyVersion());
        result += fix_imports(lines, target, d.package.toString());
    }
    {
        auto d = dep;
        result += fix_imports(lines, target, d.package.toString("::") + "-" + d.version.toAnyVersion());
        d.version.patch = -1;
        result += fix_imports(lines, target, d.package.toString("::") + "-" + d.version.toAnyVersion());
        d.version.minor = -1;
        result += fix_imports(lines, target, d.package.toString("::") + "-" + d.version.toAnyVersion());
        result += fix_imports(lines, target, d.package.toString("::"));
    }
    {
        Lines aliases;
        boost::algorithm::trim(aliases_s);
        boost::algorithm::split(aliases, aliases_s, boost::is_any_of(";"));
        for (auto &a : aliases)
            boost::algorithm::trim(a);
        for (auto &a : aliases)
            if (!a.empty())
                result += fix_imports(lines, target, a);
    }

    boost::algorithm::replace_all(result, "\r", "");
    ofile << result;
}
