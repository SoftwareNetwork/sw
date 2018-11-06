// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "generator.h"

#include "solution.h"

#include <filesystem.h>

#include <primitives/sw/settings.h>
#include <primitives/win32helpers.h>

#include <boost/algorithm/string.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <sstream>
#include <stack>

//extern cl::SubCommand subcommand_ide;
static cl::opt<bool> print_dependencies("print-dependencies"/*, cl::sub(subcommand_ide)*/);

namespace sw
{

String toPathString(GeneratorType t)
{
    switch (t)
    {
    case GeneratorType::VisualStudio:
        return "vs";
    case GeneratorType::VisualStudioNMake:
        return "vs_nmake";
    case GeneratorType::Ninja:
        return "ninja";
    default:
        throw std::logic_error("not implemented");
    }
}

String toString(GeneratorType t)
{
    throw std::logic_error("not implemented");
}

GeneratorType fromString(const String &s)
{
    // make icasecmp
    if (boost::iequals(s, "VS"))
        return GeneratorType::VisualStudio;
    else if (boost::iequals(s, "VS_NMake"))
        return GeneratorType::VisualStudioNMake;
    else if (boost::iequals(s, "Ninja"))
        return GeneratorType::Ninja;
    //else if (boost::iequals(s, "qtc"))
        //return GeneratorType::qtc;
    return GeneratorType::UnspecifiedGenerator;
}

std::unique_ptr<Generator> Generator::create(const String &s)
{
    auto t = fromString(s);
    std::unique_ptr<Generator> g;
    switch (t)
    {
    case GeneratorType::VisualStudio:
        g = std::make_unique<VSGenerator>();
        break;
    case GeneratorType::VisualStudioNMake:
        g = std::make_unique<VSGeneratorNMake>();
        break;
    case GeneratorType::Ninja:
        g = std::make_unique<NinjaGenerator>();
        break;
    default:
        throw std::logic_error("not implemented");
    }
    g->type = t;
    return g;
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
    return boost::to_upper_copy(ss.str());
}

String make_backslashes(String s)
{
    std::replace(s.begin(), s.end(), '/', '\\');
    return s;
}

static const std::map<ConfigurationType, String> configs{
    {ConfigurationType::Debug,"Debug",},
    {ConfigurationType::Release,"Release",},
    //{ConfigurationType::MinimalSizeRelease,"MinSizeRel",},
    //{ConfigurationType::ReleaseWithDebugInformation,"RelWithDebInfo",},
};

static const std::map<ArchType, String> platforms{
    {ArchType::x86,"Win32",},
    {ArchType::x86_64,"x64",},
};

static const std::map<LibraryType, String> shared_static{
    //{LibraryType::Static,"static",},
    //{LibraryType::Shared,"dll",},
};

enum class VSProjectType
{
    Directory,
    Makefile,
    Application,
    DynamicLibrary,
    StaticLibrary,
    Utility,
};

static std::map<VSProjectType, String> project_type_uuids{
    {VSProjectType::Directory,"{2150E333-8FDC-42A3-9474-1A3956D46DE8}",},

    // other?
    {VSProjectType::Makefile,"{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}",},
    {VSProjectType::Application,"{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}",},
    {VSProjectType::DynamicLibrary,"{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}",},
    {VSProjectType::StaticLibrary,"{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}",},
    {VSProjectType::Utility,"{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}",},
};

String add_space_if_not_empty(const String &s)
{
    if (s.empty())
        return {};
    return " " + s;
}

void iterate_over_configs(std::function<void(const String &, const String &, const String &)> f)
{
    for (auto &p : platforms)
        for (auto &c : configs)
            if (shared_static.empty())
                f(c.second, p.second, {});
            else
            for (auto &dll : shared_static)
                f(c.second, p.second, dll.second);
}

void iterate_over_configs(TargetBase::SettingsX s, std::function<void(const TargetBase::SettingsX &, const String &, const String &, const String &)> f)
{
    for (auto &p : platforms)
    {
        s.TargetOS.Arch = p.first;
        for (auto &c : configs)
        {
            s.Native.ConfigurationType = c.first;
            if (shared_static.empty())
                f(s, c.second, p.second, {});
            else
            for (auto &dll : shared_static)
            {
                s.Native.LibrariesType = dll.first;
                f(s, c.second, p.second, dll.second);
            }
        }
    }
}

// VS
struct SolutionContext : primitives::Context
{
    struct Project
    {
        std::unique_ptr<SolutionContext> ctx;
        std::set<String> deps;

        Project()
        {
            ctx = std::make_unique<SolutionContext>(false);
        }
    };

    using Base = primitives::Context;

    mutable std::unordered_map<String, String> uuids;
    std::map<String, Project> projects;

    SolutionContext(bool print_version = true)
        : Context("\t")
    {
        if (print_version)
            printVersion();
    }

    void printVersion()
    {
        addLine();
        addLine("Microsoft Visual Studio Solution File, Format Version 12.00");
        addLine("# Visual Studio 15");
        addLine("VisualStudioVersion = 15.0.28010.2046");
        addLine("MinimumVisualStudioVersion = 10.0.40219.1");
    }

    void addDirectory(const String &display_name, const String &solution_dir = {})
    {
        addDirectory(display_name, display_name, solution_dir);
    }

    void addDirectory(const InsecurePath &n, const String &display_name, const String &solution_dir = {})
    {
        auto up = boost::uuids::random_generator()();
        uuids[n.toString()] = uuid2string(up);

        addLine("Project(\"" + project_type_uuids[VSProjectType::Directory] + "\") = \"" +
            display_name + "\", \"" + n.toString("\\") + "\", \"{" + uuids[n] + "}\"");
        addLine("EndProject");

        if (!solution_dir.empty())
            nested_projects[n.toString()] = solution_dir;
    }

    void addProject(VSProjectType type, const String &n, const path &dir, const String &solution_dir)
    {
        beginProject(type, n, dir, solution_dir);
        endProject();
    }

    void beginProject(VSProjectType type, const String &n, const path &dir, const String &solution_dir)
    {
        auto up = boost::uuids::random_generator()();
        uuids[n] = uuid2string(up);

        beginBlock("Project(\"" + project_type_uuids[type] + "\") = \"" +
            n + "\", \"" + (dir / (n + ".vcxproj")).u8string() + "\", \"{" + uuids[n] + "}\"");

        addLine(*projects[n].ctx);

        if (!solution_dir.empty())
            nested_projects[n] = solution_dir;
    }

    void endProject()
    {
        endBlock("EndProject");
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
        printNestedProjects();

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
        iterate_over_configs([this](const String &c, const String &p, const String &dll)
        {
            addLine(c + add_space_if_not_empty(dll) + "|" + p + " = " + c + add_space_if_not_empty(dll) + "|" + p);
        });
        endGlobalSection();
    }

    void addProjectConfigurationPlatforms(const String &prj, bool build = false)
    {
        iterate_over_configs([this, &prj, build](const String &c, const String &p, const String &dll)
        {
            addKeyValue(getStringUuid(prj) + "." + c + add_space_if_not_empty(dll) + "|" + p + ".ActiveCfg", c + add_space_if_not_empty(dll) + "|" + p);
            if (build)
                addKeyValue(getStringUuid(prj) + "." + c + add_space_if_not_empty(dll) + "|" + p + ".Build.0", c + add_space_if_not_empty(dll) + "|" + p);
        });
    }

    void beginProjectSection(const String &n, const String &disposition)
    {
        beginBlock("ProjectSection(" + n + ") = " + disposition);
    }

    void endProjectSection()
    {
        endBlock("EndProjectSection");
    }

    void addKeyValue(const String &k, const String &v)
    {
        addLine(k + " = " + v);
    }

    String getStringUuid(const String &k) const
    {
        return "{" + uuids[k] + "}";
    }

    Text getText() const override
    {
        for (auto &[n, p] : projects)
        {
            if (p.deps.empty())
                continue;
            p.ctx->beginProjectSection("ProjectDependencies", "postProject");
            for (auto &d : p.deps)
                p.ctx->addLine(getStringUuid(d) + " = " + getStringUuid(d));
            p.ctx->endProjectSection();
        }
        return Base::getText();
    }

private:
    std::map<String, String> nested_projects;

    void printNestedProjects()
    {
        beginGlobalSection("NestedProjects", "preSolution");
        for (auto &[k,v]: nested_projects)
            addKeyValue(getStringUuid(k), getStringUuid(v));
        endGlobalSection();
    }
};

struct XmlContext : primitives::Context
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
        iterate_over_configs([this](const String &c, const String &p, const String &dll)
        {
            beginBlock("ProjectConfiguration", { {"Include", c + add_space_if_not_empty(dll) + "|" + p } });
            addBlock("Configuration", c + add_space_if_not_empty(dll));
            addBlock("Platform", p);
            endBlock();
        });
        endBlock();
    }

    void addPropertyGroupConfigurationTypes()
    {
        iterate_over_configs([this](const String &c, const String &p, const String &dll)
        {
            beginBlock("PropertyGroup", { { "Condition", "'$(Configuration)|$(Platform)'=='" + c + add_space_if_not_empty(dll) + "|" + p + "'" },{ "Label","Configuration" } });

            addBlock("ConfigurationType", "Makefile");
            //addBlock("UseDebugLibraries", c);
            addBlock("PlatformToolset", "v141");

            endBlock();
        });
    }

    void addPropertySheets()
    {
        iterate_over_configs([this](const String &c, const String &p, const String &dll)
        {
            beginBlock("ImportGroup", { { "Condition", "'$(Configuration)|$(Platform)'=='" + c + add_space_if_not_empty(dll) + "|" + p + "'" },{ "Label","PropertySheets" } });
            addBlock("Import", "", { {"Project","$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props" },{ "Condition","exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')" },{ "Label","LocalAppDataPlatform" }, });
            endBlock();
        });
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

struct PackagePathTree
{
    using Directories = std::set<PackagePath>;

    std::map<String, PackagePathTree> tree;

    void add(const PackagePath &p)
    {
        if (p.empty())
            return;
        tree[p.slice(0, 1).toString()].add(p.slice(1));
    }

    Directories getDirectories(const PackagePath &p = {})
    {
        Directories dirs;
        for (auto &[s, t] : tree)
        {
            auto dirs2 = t.getDirectories(p / s);
            dirs.insert(dirs2.begin(), dirs2.end());
        }
        if (tree.size() > 1 && !p.empty())
            dirs.insert(p);
        return dirs;
    }
};

VSGenerator::VSGenerator()
{
    cwd = "\"" + current_thread_path().string() + "\"";
}

String getLatestWindowsKit();

void VSGenerator::generate(const Build &b)
{
    dir = b.getIdeDir() / toPathString(type);
    PackagePathTree tree, local_tree;
    PackagePathTree::Directories parents, local_parents;
    SolutionContext ctx;

    // add ALL_BUILD target
    {
        ctx.addDirectory(predefined_targets_dir);
        ctx.addProject(VSProjectType::Utility, all_build_name, projects_dir, predefined_targets_dir);

        ProjectContext pctx;
        pctx.beginProject();

        pctx.addProjectConfigurations();

        pctx.beginBlock("PropertyGroup", { {"Label", "Globals"} });
        pctx.addBlock("VCProjectVersion", "15.0");
        pctx.addBlock("ProjectGuid", "{" + ctx.uuids[all_build_name] + "}");
        pctx.addBlock("RootNamespace", all_build_name);
        pctx.addBlock("WindowsTargetPlatformVersion", getLatestWindowsKit());
        pctx.endBlock();

        pctx.addBlock("Import", "", { {"Project", "$(VCTargetsPath)\\Microsoft.Cpp.Default.props"} });
        //pctx.addPropertyGroupConfigurationTypes();

        iterate_over_configs(b.Settings, [this, &pctx, &b](const TargetBase::SettingsX &s, const String &c, const String &pl, const String &dll)
        {
            using namespace sw;

            pctx.beginBlock("PropertyGroup", { { "Condition", "'$(Configuration)|$(Platform)'=='" + c + add_space_if_not_empty(dll) + "|" + pl + "'" } });

            String toolset = "v141";
            auto p = b.solutions[0].findProgramByExtension(".cpp");
            if (!p)
                p = b.solutions[0].findProgramByExtension(".c");
            if (!p)
                p = b.solutions[0].findProgramByExtension(".asm");
            if (!p)
                throw std::runtime_error("Unknown lang");
            //if (auto p2 = p->as<VisualStudioCompiler>(); p2)
                //toolset = p2->toolset;
            //if (auto p2 = p->as<ClangClCompiler>(); p2)
                //;
            pctx.addBlock("ConfigurationType", "Utillity");
            pctx.addBlock("PlatformToolset", toolset);
            pctx.endBlock();
        });

        pctx.addBlock("Import", "", { { "Project", "$(VCTargetsPath)\\Microsoft.Cpp.props" } });
        pctx.addPropertySheets();

        pctx.addBlock("Import", "", { { "Project", "$(VCTargetsPath)\\Microsoft.Cpp.targets" } });

        pctx.endProject();
        write_file(dir / projects_dir / (all_build_name + ".vcxproj"), pctx.getText());
    }

    // gather parents
    bool has_deps = false;
    for (auto &[p, t] : b.solutions[0].children)
    {
        has_deps |= !t->Local;
        (t->Local ? local_tree : tree).add(p.ppath);
    }
    if (has_deps && print_dependencies)
        ctx.addDirectory(deps_subdir);

    auto add_dirs = [&ctx](auto &t, auto &prnts, const String &root = {})
    {
        for (auto &p : prnts = t.getDirectories())
        {
            auto pp = p.parent();
            while (!pp.empty() && prnts.find(pp) == prnts.end())
                pp = pp.parent();
            ctx.addDirectory(InsecurePath() / p.toString(), p.slice(pp.size()), pp.empty() ? root : pp.toString());
        }
    };
    if (print_dependencies)
        add_dirs(tree, parents, deps_subdir.toString());
    add_dirs(local_tree, local_parents);

    for (auto &[p, t] : b.solutions[0].children)
    {
        if (!print_dependencies && !t->Local)
            continue;

        auto pp = p.ppath.parent();
        auto &prnts = t->Local ? local_parents : parents;
        while (!pp.empty() && prnts.find(pp) == prnts.end())
            pp = pp.parent();
        ctx.addProject(VSProjectType::Makefile, p.target_name, projects_dir, pp);
    }

    // gen projects
    for (auto &[p, t] : b.solutions[0].children)
    {
        if (!print_dependencies && !t->Local)
            continue;

        auto nt = t->as<NativeExecutedTarget>();

        ProjectContext pctx;
        pctx.beginProject();

        pctx.addProjectConfigurations();

        // project name helper
        auto pp = p.ppath.parent();
        auto &prnts = t->Local ? local_parents : parents;
        while (!pp.empty() && prnts.find(pp) == prnts.end())
            pp = pp.parent();

        ctx.projects[all_build_name].deps.insert(p.target_name);

        pctx.beginBlock("PropertyGroup", { {"Label", "Globals"} });
        pctx.addBlock("VCProjectVersion", "15.0");
        pctx.addBlock("ProjectGuid", "{" + ctx.uuids[p.target_name] + "}");
        pctx.addBlock("RootNamespace", p.target_name);
        pctx.addBlock("WindowsTargetPlatformVersion", getLatestWindowsKit());
        //pctx.addBlock("Keyword", "Win32Proj");
        //pctx.addBlock("ProjectName", PackageId(p.ppath.slice(pp.size()), p.version).toString());
        pctx.endBlock();

        pctx.addBlock("Import", "", { {"Project", "$(VCTargetsPath)\\Microsoft.Cpp.Default.props"} });

        iterate_over_configs(nt->Settings,
            [this, &pctx, &nt, &p, &b, &t]
            (const TargetBase::SettingsX &s, const String &c, const String &pl, const String &dll)
        {
            using namespace sw;

            pctx.beginBlock("PropertyGroup", { { "Condition", "'$(Configuration)|$(Platform)'=='" + c + add_space_if_not_empty(dll) + "|" + pl + "'" } });
            String ext = ".exe";
            String type = "Application";
            switch (t->getType())
            {
            case TargetType::NativeLibrary:
                if (dll != "dll")
                {
                    type = "StaticLibrary";
                    ext = ".lib";
                }
                else
                {
                    type = "SharedLibrary";
                    ext = ".dll";
                }
                break;
            case TargetType::NativeStaticLibrary:
                type = "StaticLibrary";
                ext = ".lib";
                break;
            case TargetType::NativeSharedLibrary:
                type = "SharedLibrary";
                ext = ".dll";
                break;
            default:
                break;
            }
            pctx.addBlock("ConfigurationType", type);

            bool unicode = nt->Definitions.find("UNICODE") != nt->Definitions.end();
            pctx.addBlock("CharacterSet", unicode ? "MultiByte" : "Unicode");

            String toolset = "v141";
            auto p = b.solutions[0].findProgramByExtension(".cpp");
            if (!p)
                p = b.solutions[0].findProgramByExtension(".c");
            if (!p)
                p = b.solutions[0].findProgramByExtension(".asm");
            if (!p)
                throw std::runtime_error("Unknown lang");
            //if (auto p2 = p->as<VisualStudioCompiler>(); p2)
                //toolset = p2->toolset;
            //if (auto p2 = p->as<ClangClCompiler>(); p2)
                //;
            pctx.addBlock("PlatformToolset", toolset);
            pctx.endBlock();
        });

        //pctx.addPropertyGroupConfigurationTypes();
        pctx.addBlock("Import", "", { { "Project", "$(VCTargetsPath)\\Microsoft.Cpp.props" } });
        pctx.addPropertySheets();

        iterate_over_configs(nt->Settings,
            [this, &pctx, &nt, &p, &b, &t]
        (const TargetBase::SettingsX &s, const String &c, const String &pl, const String &dll)
        {
            using namespace sw;

            String ext = ".exe";
            String type = "Application";
            switch (t->getType())
            {
            case TargetType::NativeLibrary:
                if (dll != "dll")
                {
                    type = "StaticLibrary";
                    ext = ".lib";
                }
                else
                {
                    type = "SharedLibrary";
                    ext = ".dll";
                }
                break;
            case TargetType::NativeStaticLibrary:
                type = "StaticLibrary";
                ext = ".lib";
                break;
            case TargetType::NativeSharedLibrary:
                type = "SharedLibrary";
                ext = ".dll";
                break;
            default:
                break;
            }

            pctx.beginBlock("PropertyGroup");
            {
                pctx.addBlock("OutDir", normalize_path_windows(current_thread_path() / "bin\\"),
                    { { "Condition", "'$(Configuration)|$(Platform)'=='" + c + add_space_if_not_empty(dll) + "|" + pl + "'" } });
                pctx.addBlock("IntDir", normalize_path_windows(dir / projects_dir / sha256_short(nt->pkg.target_name)) + "\\",
                    { { "Condition", "'$(Configuration)|$(Platform)'=='" + c + add_space_if_not_empty(dll) + "|" + pl + "'" } });
                pctx.addBlock("TargetName", nt->pkg.toString(), { { "Condition", "'$(Configuration)|$(Platform)'=='" + c + add_space_if_not_empty(dll) + "|" + pl + "'" } });
                pctx.addBlock("TargetExt", ext, { { "Condition", "'$(Configuration)|$(Platform)'=='" + c + add_space_if_not_empty(dll) + "|" + pl + "'" } });
            }
            pctx.endBlock();

            /*pctx.beginBlock("ItemDefinitionGroup", { { "Condition", "'$(Configuration)|$(Platform)'=='" + c + add_space_if_not_empty(dll) + "|" + pl + "'" } });
            pctx.beginBlock("ClCompile");
            pctx.endBlock();
            pctx.beginBlock("Link");
            pctx.endBlock();
            pctx.endBlock();*/
        });

        pctx.beginBlock("ItemGroup");
        for (auto &[p, sf] : *nt)
        {
            if (sf->skip)
                continue;
            pctx.beginBlock("ClCompile", { { "Include", p.string() } });
            pctx.endBlock();
        }
        pctx.endBlock();

        pctx.addBlock("Import", "", { { "Project", "$(VCTargetsPath)\\Microsoft.Cpp.targets" } });

        pctx.endProject();
        write_file(dir / projects_dir / (p.target_name + ".vcxproj"), pctx.getText());

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
                auto ss = fd.substr(p);
                if (ss[0] == '/')
                    ss = ss.substr(1);
                path r = ss;
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
        write_file(dir / projects_dir / (p.target_name + ".vcxproj.filters"), fctx.getText());
    }

    ctx.beginGlobal();
    ctx.setSolutionConfigurationPlatforms();
    ctx.beginGlobalSection("ProjectConfigurationPlatforms", "postSolution");
    for (auto &[p, t] : b.solutions[0].children)
    {
        if (!print_dependencies && !t->Local)
            continue;
        ctx.addProjectConfigurationPlatforms(p.target_name);
    }
    ctx.addProjectConfigurationPlatforms(all_build_name, true);
    ctx.endGlobalSection();
    ctx.endGlobal();

    const auto compiler_name = boost::to_lower_copy(toString(b.Settings.Native.CompilerType));
    String fn = "sw_";
    fn += compiler_name + "_" + toPathString(type);
    fn += ".sln";
    write_file(dir / fn, ctx.getText());
    auto lnk = current_thread_path() / fn;
    lnk += ".lnk";
    ::create_link(dir / fn, lnk, "SW link");
}

void VSGeneratorNMake::generate(const Build &b)
{
    dir = b.getIdeDir() / toPathString(type);
    PackagePathTree tree, local_tree;
    PackagePathTree::Directories parents, local_parents;
    SolutionContext ctx;

    // add ALL_BUILD target
    {
        ctx.addDirectory(predefined_targets_dir);
        ctx.addProject(VSProjectType::Makefile, all_build_name, projects_dir, predefined_targets_dir);

        ProjectContext pctx;
        pctx.beginProject();

        pctx.addProjectConfigurations();

        pctx.beginBlock("PropertyGroup", { {"Label", "Globals"} });
        pctx.addBlock("VCProjectVersion", "15.0");
        pctx.addBlock("ProjectGuid", "{" + ctx.uuids[all_build_name] + "}");
        pctx.addBlock("Keyword", "Win32Proj");
        pctx.addBlock("ProjectName", all_build_name);
        pctx.endBlock();

        pctx.addBlock("Import", "", { {"Project", "$(VCTargetsPath)\\Microsoft.Cpp.Default.props"} });
        pctx.addPropertyGroupConfigurationTypes();
        pctx.addBlock("Import", "", { { "Project", "$(VCTargetsPath)\\Microsoft.Cpp.props" } });
        pctx.addPropertySheets();

        iterate_over_configs(b.Settings, [this, &pctx, &b](const TargetBase::SettingsX &s, const String &c, const String &pl, const String &dll)
        {
            using namespace sw;

            pctx.beginBlock("PropertyGroup", { { "Condition", "'$(Configuration)|$(Platform)'=='" + c + add_space_if_not_empty(dll) + "|" + pl + "'" } });

            String cfg = "--configuration " + c + " --platform " + pl;
            if (dll != "dll")
                cfg += " --static-build";

            String compiler;
            if (b.Settings.Native.CompilerType == CompilerType::Clang)
                compiler = "--compiler clang";
            else if (b.Settings.Native.CompilerType == CompilerType::ClangCl)
                compiler = "--compiler clang-cl";
            else if (b.Settings.Native.CompilerType == CompilerType::GNU)
                compiler = "--compiler gnu";
            else if (b.Settings.Native.CompilerType == CompilerType::MSVC)
                compiler = "--compiler msvc";

            pctx.addBlock("NMakeBuildCommandLine", "sw -d " + cwd + " " + cfg + " " + compiler + " --do-not-rebuild-config ide");
            pctx.addBlock("NMakeCleanCommandLine", "sw -d " + cwd + " " + cfg + " ide --clean");
            pctx.addBlock("NMakeReBuildCommandLine", "sw -d " + cwd + " " + cfg + " " + compiler + " ide --rebuild");

            pctx.endBlock();
        });

        pctx.addBlock("Import", "", { { "Project", "$(VCTargetsPath)\\Microsoft.Cpp.targets" } });

        pctx.endProject();
        write_file(dir / projects_dir / (all_build_name + ".vcxproj"), pctx.getText());
    }

    // gather parents
    bool has_deps = false;
    for (auto &[p, t] : b.solutions[0].children)
    {
        has_deps |= !t->Local;
        (t->Local ? local_tree : tree).add(p.ppath);
    }
    if (has_deps && print_dependencies)
        ctx.addDirectory(deps_subdir);

    auto add_dirs = [&ctx](auto &t, auto &prnts, const String &root = {})
    {
        for (auto &p : prnts = t.getDirectories())
        {
            auto pp = p.parent();
            while (!pp.empty() && prnts.find(pp) == prnts.end())
                pp = pp.parent();
            ctx.addDirectory(InsecurePath() / p.toString(), p.slice(pp.size()), pp.empty() ? root : pp.toString());
        }
    };
    if (print_dependencies)
        add_dirs(tree, parents, deps_subdir.toString());
    add_dirs(local_tree, local_parents);

    for (auto &[p, t] : b.solutions[0].children)
    {
        if (!print_dependencies && !t->Local)
            continue;

        auto pp = p.ppath.parent();
        auto &prnts = t->Local ? local_parents : parents;
        while (!pp.empty() && prnts.find(pp) == prnts.end())
            pp = pp.parent();
        ctx.addProject(VSProjectType::Makefile, p.target_name, projects_dir, pp);
    }

    // gen projects
    for (auto &[p, t] : b.solutions[0].children)
    {
        if (!print_dependencies && !t->Local)
            continue;

        auto nt = t->as<NativeExecutedTarget>();

        ProjectContext pctx;
        pctx.beginProject();

        pctx.addProjectConfigurations();

        /*
          <PropertyGroup Label="Globals">
            <WindowsTargetPlatformVersion>10.0.17134.0</WindowsTargetPlatformVersion>
            <Platform>Win32</Platform>
          </PropertyGroup>
        */

        // project name helper
        auto pp = p.ppath.parent();
        auto &prnts = t->Local ? local_parents : parents;
        while (!pp.empty() && prnts.find(pp) == prnts.end())
            pp = pp.parent();

        pctx.beginBlock("PropertyGroup", { {"Label", "Globals"} });
        pctx.addBlock("VCProjectVersion", "15.0");
        pctx.addBlock("ProjectGuid", "{" + ctx.uuids[p.target_name] + "}");
        pctx.addBlock("Keyword", "Win32Proj");
        pctx.addBlock("ProjectName", PackageId(p.ppath.slice(pp.size()), p.version).toString());
        pctx.endBlock();

        pctx.addBlock("Import", "", { {"Project", "$(VCTargetsPath)\\Microsoft.Cpp.Default.props"} });
        pctx.addPropertyGroupConfigurationTypes();
        pctx.addBlock("Import", "", { { "Project", "$(VCTargetsPath)\\Microsoft.Cpp.props" } });
        pctx.addPropertySheets();

        iterate_over_configs(nt->Settings, [this, &pctx, &nt, &p, &b, &t](const TargetBase::SettingsX &s, const String &c, const String &pl, const String &dll)
        {
            using namespace sw;

            pctx.beginBlock("PropertyGroup", { { "Condition", "'$(Configuration)|$(Platform)'=='" + c + add_space_if_not_empty(dll) + "|" + pl + "'" } });

            String cfg = "--configuration " + c + " --platform " + pl;
            if (dll != "dll")
                cfg += " --static-build";

            String compiler;
            if (b.Settings.Native.CompilerType == CompilerType::Clang)
                compiler = "--compiler clang";
            else if (b.Settings.Native.CompilerType == CompilerType::ClangCl)
                compiler = "--compiler clang-cl";
            else if (b.Settings.Native.CompilerType == CompilerType::GNU)
                compiler = "--compiler gnu";

            auto o = nt->getOutputFile();
            o = o.parent_path().parent_path() / s.getConfig(t.get()) / o.filename();
            pctx.addBlock("NMakeBuildCommandLine", "sw -d " + cwd + " " + cfg + " " + compiler + " --do-not-rebuild-config --target " + p.target_name + " ide");
            pctx.addBlock("NMakeOutput", o.u8string());
            pctx.addBlock("NMakeCleanCommandLine", "sw -d " + cwd + " " + cfg + " ide --clean");
            pctx.addBlock("NMakeReBuildCommandLine", "sw -d " + cwd + " " + cfg + " " + compiler + " ide --rebuild");
            String defs;
            for (auto &[k, v] : nt->Definitions)
            {
                if (v.empty())
                    defs += k + ";";
                else
                    defs += k + "=" + v + ";";
            }
            pctx.addBlock("NMakePreprocessorDefinitions", defs);
            String idirs;
            for (auto &i : nt->gatherIncludeDirectories())
                idirs += i.string() + ";";
            pctx.addBlock("NMakeIncludeSearchPath", idirs);
            //pctx.addBlock("NMakeForcedIncludes", "Makefile");
            //pctx.addBlock("NMakeAssemblySearchPath", "Makefile");
            //pctx.addBlock("NMakeForcedUsingAssemblies", "Makefile");

            pctx.endBlock();
        });

        pctx.beginBlock("ItemGroup");
        for (auto &[p, sf] : *nt)
        {
            if (sf->skip)
                continue;
            pctx.beginBlock("ClCompile", { { "Include", p.string() } });
            pctx.endBlock();
        }
        pctx.endBlock();

        pctx.addBlock("Import", "", { { "Project", "$(VCTargetsPath)\\Microsoft.Cpp.targets" } });

        pctx.endProject();
        write_file(dir / projects_dir / (p.target_name + ".vcxproj"), pctx.getText());

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
                auto ss = fd.substr(p);
                if (ss[0] == '/')
                    ss = ss.substr(1);
                path r = ss;
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
        write_file(dir / projects_dir / (p.target_name + ".vcxproj.filters"), fctx.getText());
    }

    ctx.beginGlobal();
    ctx.setSolutionConfigurationPlatforms();
    ctx.beginGlobalSection("ProjectConfigurationPlatforms", "postSolution");
    for (auto &[p, t] : b.solutions[0].children)
    {
        if (!print_dependencies && !t->Local)
            continue;
        ctx.addProjectConfigurationPlatforms(p.target_name);
    }
    ctx.addProjectConfigurationPlatforms(all_build_name, true);
    ctx.endGlobalSection();
    ctx.endGlobal();

    const auto compiler_name = boost::to_lower_copy(toString(b.Settings.Native.CompilerType));
    String fn = "sw_";
    fn += compiler_name + "_" + toPathString(type);
    fn += ".sln";
    write_file(dir / fn, ctx.getText());
    auto lnk = current_thread_path() / fn;
    lnk += ".lnk";
    ::create_link(dir / fn, lnk, "SW link");
}

struct NinjaContext : primitives::Context
{
    void addCommand(const Build &b, const path &dir, builder::Command *c)
    {
        String command;

        auto prog = c->getProgram().u8string();
        if (prog == "ExecuteCommand")
            return;

        bool rsp = c->needsResponseFile();
        path rsp_dir = dir / "rsp";
        path rsp_file = fs::absolute(rsp_dir / ("rsp" + std::to_string(c->getHash()) + ".rsp"));
        if (rsp)
            fs::create_directories(rsp_dir);

        auto has_mmd = false;

        addLine("rule c" + std::to_string(c->getHash()));
        increaseIndent();
        // add cmd /C ""
        addLine("command = ");
        if (b.Settings.TargetOS.Type == OSType::Windows)
        {
            addText("cmd /S /C ");
            addText("\"");
        }
        //else
            //addText("bash -c ");
        if (!c->working_directory.empty())
        {
            addText("cd ");
            if (b.Settings.TargetOS.Type == OSType::Windows)
                addText("/D ");
            addText(prepareString(b, getShortName(c->working_directory), true) + " && ");
        }
        addText(prepareString(b, getShortName(prog), true) + " ");
        if (!rsp)
        {
            for (auto &a : c->args)
            {
                addText(prepareString(b, a, true) + " ");
                has_mmd |= "-MMD" == a;
            }
        }
        else
        {
            addText("@" + rsp_file.u8string() + " ");
        }
        if (!c->out.file.empty())
            addText("> " + prepareString(b, getShortName(c->out.file), true) + " ");
        if (!c->err.file.empty())
            addText("2> " + prepareString(b, getShortName(c->err.file), true) + " ");
        if (b.Settings.TargetOS.Type == OSType::Windows)
            addText("\"");
        if (prog.find("cl.exe") != prog.npos)
            addLine("deps = msvc");
        if (b.Settings.Native.CompilerType == CompilerType::GCC && has_mmd)
            addLine("depfile = " + (c->outputs.begin()->parent_path() / (c->outputs.begin()->stem().string() + ".d")).u8string());
        if (rsp)
        {
            addLine("rspfile = " + rsp_file.u8string());
            addLine("rspfile_content = ");
            for (auto &a : c->args)
                addText(prepareString(b, a, true) + " ");
        }
        decreaseIndent();
        addLine();

        addLine("build ");
        for (auto &o : c->outputs)
            addText(prepareString(b, getShortName(o)) + " ");
        for (auto &o : c->intermediate)
            addText(prepareString(b, getShortName(o)) + " ");
        addText(": c" + std::to_string(c->getHash()) + " ");
        for (auto &i : c->inputs)
            addText(prepareString(b, getShortName(i)) + " ");
        addLine();
    }

private:
    String getShortName(const path &p)
    {
#ifdef _WIN32
        std::wstring buf(4096, 0);
        path p2 = normalize_path_windows(p);
        if (!GetShortPathName(p2.wstring().c_str(), buf.data(), buf.size()))
            //throw std::runtime_error("GetShortPathName failed for path: " + p.u8string());
            return p.u8string();
        return to_string(buf);
#else
        return p.u8string();
#endif
    }

    String prepareString(const Build &b, const String &s, bool quotes = false)
    {
        if (b.Settings.TargetOS.Type != OSType::Windows)
            quotes = false;

        auto s2 = s;
        boost::replace_all(s2, ":", "$:");
        boost::replace_all(s2, "\"", "\\\"");
        if (quotes)
            return "\"" + s2 + "\"";
        return s2;
    }
};

void NinjaGenerator::generate(const Build &b)
{
    // https://ninja-build.org/manual.html#_writing_your_own_ninja_files

    const auto dir = path(".sw") / toPathString(type) / b.getConfig();

    NinjaContext ctx;

    auto ep = b.getExecutionPlan();
    for (auto &c : ep.commands)
        ctx.addCommand(b, dir, c.get());

    auto t = ctx.getText();
    //if (b.Settings.TargetOS.Type != OSType::Windows)
        //std::replace(t.begin(), t.end(), '\\', '/');

    write_file(dir / "build.ninja", t);
}

}
