// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "generator.h"

#include "solution.h"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <primitives/context.h>

#include <sstream>
#include <stack>

namespace sw
{

String toString(GeneratorType Type)
{
    throw std::logic_error("not implemented");
}

GeneratorType fromString(const String &s)
{
    throw std::logic_error("not implemented");
}

void Generator::generate(const path &f, const Build &b)
{
    file = f;
    generate(b);
}

String uuid2string(const boost::uuids::uuid &u)
{
    std::ostringstream ss;
    ss << u;
    return ss.str();
}

String make_backslashes(String s)
{
    std::replace(s.begin(), s.end(), '/', '\\');
    return s;
}

static const Strings configs{
    "Debug",
    "Release",
    "MinSizeRel",
    "RelWithDebInfo",
};

static const Strings platforms{
    "x86",
    "x64",
};

// VS
struct SolutionContext : Context
{
    std::unordered_map<String, String> uuids;

    SolutionContext()
        : Context("\t")
    {
        addLine("Microsoft Visual Studio Solution File, Format Version 12.00");
        addLine("# Visual Studio 15");
    }

    void addProject(const String &n)
    {
        auto u = boost::uuids::random_generator()();
        auto up = boost::uuids::random_generator()();
        uuids[n] = uuid2string(up);

        addLine("Project(\"{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}\") = \"" + n + "\", \"" + n + ".vcxproj\", \"{" + uuids[n] + "}\"");
        addLine("EndProject");
    }

    void beginBlock(const String &s)
    {
        addLine(s);
        increaseIndent();
    }

    void endBlock(const String &s)
    {
        decreaseIndent();
        addLine(s);
    }

    void beginGlobal()
    {
        beginBlock("Global");
    }

    void endGlobal()
    {
        endBlock("EndGlobal");
    }

    void beginGlobalSection(const String &name, const String &post)
    {
        beginBlock("GlobalSection(" + name + ") = " + post);
    }

    void endGlobalSection()
    {
        endBlock("EndGlobalSection");
    }

    void setSolutionConfigurationPlatforms()
    {
        beginGlobalSection("SolutionConfigurationPlatforms", "preSolution");
        for (auto &c : configs)
        {
            for (auto &p : platforms)
                addLine(c + "|" + p + " = " + c + "|" + p);
        }
        endGlobalSection();
    }

    void addProjectConfigurationPlatforms(const String &prj)
    {
        for (auto &c : configs)
        {
            for (auto &p : platforms)
            {
                addLine("{" + uuids[prj] + "}." + c + "|" + p + ".ActiveCfg = " + c + "|" + p);
                addLine("{" + uuids[prj] + "}." + c + "|" + p + ".Build.0 = " + c + "|" + p);
            }
        }
    }
};

struct XmlContext : Context
{
    std::stack<String> blocks;

    XmlContext()
        : Context("  ")
    {
        addLine(R"(<?xml version="1.0" encoding="utf-8"?>)");
    }

    void beginBlock(const String &n, const std::map<String, String> &params = std::map<String, String>(), bool empty = false)
    {
        beginBlock1(n, params, empty);
        increaseIndent();
    }

    void endBlock()
    {
        decreaseIndent();
        endBlock1();
    }

    void addBlock(const String &n, const String &v, const std::map<String, String> &params = std::map<String, String>())
    {
        beginBlock1(n, params, v.empty());
        if (v.empty())
            return;
        addText(v);
        endBlock1(true);
    }

protected:
    void beginBlock1(const String &n, const std::map<String, String> &params = std::map<String, String>(), bool empty = false)
    {
        blocks.push(n);
        addLine("<" + blocks.top());
        for (auto &[k, v] : params)
            addText(" " + k + "=\"" + v + "\"");
        if (empty)
            addText(" /");
        addText(">");
        if (empty)
            blocks.pop();
    }

    void endBlock1(bool text = false)
    {
        if (text)
            addText("</" + blocks.top() + ">");
        else
            addLine("</" + blocks.top() + ">");
        blocks.pop();
    }
};

struct ProjectContext : XmlContext
{
    void beginProject()
    {
        beginBlock("Project", { {"DefaultTargets", "Build"},
                               {"ToolsVersion", "15.0"},
                               {"xmlns", "http://schemas.microsoft.com/developer/msbuild/2003"} });
    }

    void endProject()
    {
        endBlock();
    }

    void addProjectConfigurations()
    {
        beginBlock("ItemGroup", { {"Label","ProjectConfigurations"} });
        for (auto &c : configs)
        {
            for (auto &p : platforms)
            {
                beginBlock("ProjectConfiguration", { {"Include", c + "|" + p } });
                addBlock("Configuration", c);
                addBlock("Platform", p == "x86" ? "Win32" : "x64");
                endBlock();
            }
        }
        endBlock();
    }

    void addPropertyGroupConfigurationTypes()
    {
        for (auto &c : configs)
        {
            for (auto &p : platforms)
            {
                beginBlock("PropertyGroup", { { "Condition", "'$(Configuration)|$(Platform)'=='" + c + "|" + p + "'" },{ "Label","Configuration" } });

                addBlock("ConfigurationType", "Makefile");
                //addBlock("UseDebugLibraries", c);
                addBlock("PlatformToolset", "v141");

                endBlock();
            }
        }
    }

    void addPropertySheets()
    {
        for (auto &c : configs)
        {
            for (auto &p : platforms)
            {
                beginBlock("ImportGroup", { { "Condition", "'$(Configuration)|$(Platform)'=='" + c + "|" + p + "'" },{ "Label","PropertySheets" } });
                addBlock("Import", "", { {"Project","$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props" },{ "Condition","exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')" },{ "Label","LocalAppDataPlatform" }, });
                endBlock();
            }
        }
    }
};

struct FiltersContext : XmlContext
{
    void beginProject()
    {
        beginBlock("Project", { {"ToolsVersion", "4.0"},
                               {"xmlns", "http://schemas.microsoft.com/developer/msbuild/2003"} });
    }

    void endProject()
    {
        endBlock();
    }
};

void Generator::generate(const Build &b)
{
    const auto cwd = "\"" + current_thread_path().string() + "\"";
    const path dir = ".sw";

    SolutionContext ctx;
    for (auto &[p, t] : b.solutions[0].children)
        ctx.addProject(p.target_name);

    ctx.beginGlobal();
    ctx.setSolutionConfigurationPlatforms();
    ctx.beginGlobalSection("ProjectConfigurationPlatforms", "postSolution");
    for (auto &[p, t] : b.solutions[0].children)
        ctx.addProjectConfigurationPlatforms(p.target_name);
    ctx.endGlobalSection();
    ctx.endGlobal();

    write_file(dir / "sw.sln", ctx.getText());

    // gen projects
    for (auto &[p, t] : b.solutions[0].children)
    {
        auto nt = t->as<NativeExecutedTarget>();

        ProjectContext pctx;
        pctx.beginProject();

        pctx.addProjectConfigurations();

        pctx.beginBlock("ItemGroup");
        for (auto &[p, sf] : *nt)
        {
            if (sf->skip)
                continue;
            pctx.beginBlock("ClCompile", { { "Include", p.string() } });
            pctx.endBlock();
        }
        pctx.endBlock();

        pctx.beginBlock("PropertyGroup", { {"Label", "Globals"} });
        pctx.addBlock("VCProjectVersion", "15.0");
        pctx.addBlock("ProjectGuid", "{" + ctx.uuids[p.target_name] + "}");
        pctx.addBlock("Keyword", "Win32Proj");
        pctx.endBlock();

        pctx.addBlock("Import", "", { {"Project", "$(VCTargetsPath)\\Microsoft.Cpp.Default.props"} });

        pctx.addPropertyGroupConfigurationTypes();

        pctx.addBlock("Import", "", { { "Project", "$(VCTargetsPath)\\Microsoft.Cpp.props" } });

        pctx.addPropertySheets();

        for (auto &c : configs)
        {
            for (auto &pl : platforms)
            {
                using namespace sw;

                pctx.beginBlock("PropertyGroup", { { "Condition", "'$(Configuration)|$(Platform)'=='" + c + "|" + pl + "'" } });

                pctx.addBlock("NMakeBuildCommandLine", "sw -d " + cwd);
                pctx.addBlock("NMakeOutput", nt->getOutputFile().string());
                pctx.addBlock("NMakeCleanCommandLine", "sw -d " + cwd + " ide --clean");
                pctx.addBlock("NMakeReBuildCommandLine", "sw -d " + cwd + " ide --rebuild");
                String defs;
                for (auto &[k, v] : nt->Definitions)
                    defs += k + "=" + v + ";";
                pctx.addBlock("NMakePreprocessorDefinitions", defs);
                String idirs;
                for (auto &i : nt->gatherIncludeDirectories())
                    idirs += i.string() + ";";
                pctx.addBlock("NMakeIncludeSearchPath", idirs);
                //pctx.addBlock("NMakeForcedIncludes", "Makefile");
                //pctx.addBlock("NMakeAssemblySearchPath", "Makefile");
                //pctx.addBlock("NMakeForcedUsingAssemblies", "Makefile");

                pctx.endBlock();
            }
        }

        pctx.addBlock("Import", "", { { "Project", "$(VCTargetsPath)\\Microsoft.Cpp.targets" } });

        pctx.endProject();
        write_file(dir / (p.target_name + ".vcxproj"), pctx.getText());

        FiltersContext fctx;
        fctx.beginProject();
        fctx.beginBlock("ItemGroup");

        auto sd = normalize_path(nt->SourceDir);
        auto bd = normalize_path(nt->BinaryDir);
        StringSet filters;
        for (auto &[f, sf] : *nt)
        {
            if (sf->skip)
                continue;
            auto fd = normalize_path(f);
            auto p1 = fd.find(sd);
            auto p2 = fd.find(bd);
            size_t p = -1;
            path filter;
            if (p1 != -1 || p2 != -1)
            {
                if (p1 != -1 && p2 != -1)
                    p = std::max(sd.size(), bd.size());
                else if (p1 != -1)
                    p = sd.size();
                else if (p2 != -1)
                    p = bd.size();
                path r = fd.substr(p + 1);
                do
                {
                    r = r.parent_path();
                    if (filter.empty())
                        filter = r;
                    filters.insert(r.string());
                } while (!r.empty());
            }

            fctx.beginBlock("ClCompile", { {"Include",f.string()} });
            if (!filter.empty())
                fctx.addBlock("Filter", make_backslashes(/*"Source Files\\" + */filter.string()));
            fctx.endBlock();
        }
        filters.erase("");
        fctx.endBlock();

        fctx.beginBlock("ItemGroup");
        for (auto &f : filters)
        {
            fctx.beginBlock("Filter", { { "Include", make_backslashes(/*"Source Files\\" + */f) } });
            fctx.addBlock("UniqueIdentifier", "{" + uuid2string(boost::uuids::random_generator()()) + "}");
            fctx.endBlock();
        }
        fctx.endBlock();

        fctx.endProject();
        write_file(dir / (p.target_name + ".vcxproj.filters"), fctx.getText());
    }
}

}
