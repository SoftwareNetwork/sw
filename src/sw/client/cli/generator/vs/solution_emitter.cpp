/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2019 Egor Pugin
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

#include "solution_emitter.h"

#include "vs.h"

#include <sw/driver/build_settings.h>

#include <boost/algorithm/string.hpp>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "solution_emitter");

static std::map<VSProjectType, String> project_type_uuids
{
    {
        VSProjectType::Directory,
        "{2150E333-8FDC-42A3-9474-1A3956D46DE8}",
    },

    // other?
    {
        VSProjectType::Makefile,
        "{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}",
    },
    {
        VSProjectType::Application,
        "{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}",
    },
    {
        VSProjectType::DynamicLibrary,
        "{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}",
    },
    {
        VSProjectType::StaticLibrary,
        "{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}",
    },
    {
        VSProjectType::Utility,
        "{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}",
    },
};

SolutionEmitter::SolutionEmitter()
    : Emitter("\t", "\r\n")
{
}

void SolutionEmitter::printVersion()
{
    addLine("Microsoft Visual Studio Solution File, Format Version 12.00");
    if (version.getMajor() == 15)
    {
        addLine("# Visual Studio " + std::to_string(version.getMajor()));
        addLine("VisualStudioVersion = 15.0.28010.2046");
    }
    else if (version.getMajor() == 16)
    {
        addLine("# Visual Studio Version " + std::to_string(version.getMajor()));
        addLine("VisualStudioVersion = 16.0.28606.126");
    }
    else
        LOG_WARN(logger, "Unknown vs version " << version.toString());
    addLine("MinimumVisualStudioVersion = 10.0.40219.1");
}

void SolutionEmitter::addDirectory(const Directory &d)
{
    beginBlock("Project(\"" + project_type_uuids[d.type] + "\") = \"" +
        d.name + "\", \"" + d.name + "\", \"" + d.uuid + "\"");
    if (!d.files.empty())
    {
        beginBlock("ProjectSection(SolutionItems) = preProject");
        for (auto &f : d.files)
            addLine(normalize_path(f) + " = " + normalize_path(f));
        endBlock("EndProjectSection");
    }
    endBlock("EndProject");
}

void SolutionEmitter::beginProject(const Project &p)
{
    beginBlock("Project(\"" + project_type_uuids[p.type] + "\") = \"" + p.name +
        "\", \"" + (vs_project_dir / (p.name + vs_project_ext)).u8string() + "\", \"" + p.uuid + "\"");
}

void SolutionEmitter::endProject()
{
    endBlock("EndProject");
}

void SolutionEmitter::beginBlock(const String &s)
{
    addLine(s);
    increaseIndent();
}

void SolutionEmitter::endBlock(const String &s)
{
    decreaseIndent();
    addLine(s);
}

void SolutionEmitter::beginGlobal()
{
    beginBlock("Global");
}

void SolutionEmitter::endGlobal()
{
    endBlock("EndGlobal");
}

void SolutionEmitter::beginGlobalSection(const String &name, const String &post)
{
    beginBlock("GlobalSection(" + name + ") = " + post);
}

void SolutionEmitter::endGlobalSection()
{
    endBlock("EndGlobalSection");
}

struct less
{
    bool operator()(const String &s1, const String &s2) const
    {
        return boost::ilexicographical_compare(s1, s2);
    }
};

void SolutionEmitter::setSolutionConfigurationPlatforms(const Solution &s)
{
    // sort like VS does
    beginGlobalSection("SolutionConfigurationPlatforms", "preSolution");
    std::set<String, less> platforms;
    for (auto &s : s.getSettings())
        platforms.insert(get_project_configuration(s) + " = " + get_project_configuration(s));
    for (auto &s : platforms)
        addLine(s);
    endGlobalSection();
}

void SolutionEmitter::addProjectConfigurationPlatforms(const Project &p, bool build)
{
    // sort like VS does
    std::map<String, String, less> platforms;
    for (auto &s : p.getSettings())
    {
        platforms[p.uuid + "." + get_project_configuration(s) + ".ActiveCfg"] = get_project_configuration(s);
        if (build)
            platforms[p.uuid + "." + get_project_configuration(s) + ".Build.0"] = get_project_configuration(s);
    }
    for (auto &[k, v] : platforms)
        addKeyValue(k, v);
}

void SolutionEmitter::addKeyValue(const String &k, const String &v)
{
    addLine(k + " = " + v);
}

void SolutionEmitter::beginProjectSection(const String &n, const String &disposition)
{
    beginBlock("ProjectSection(" + n + ") = " + disposition);
}

void SolutionEmitter::endProjectSection()
{
    endBlock("EndProjectSection");
}
