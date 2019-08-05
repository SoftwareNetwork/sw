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

#include "vs.h"

#include "../generator.h"
#include "project_emitter.h"
#include "solution_emitter.h"

#include <sw/builder/execution_plan.h>
#include <sw/core/build.h>
#include <sw/core/input.h>
#include <sw/core/sw_context.h>
#include <sw/driver/build_settings.h>
#include <sw/support/filesystem.h>

#include <primitives/win32helpers.h>

#include <boost/algorithm/string.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <sstream>
#include <stack>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "generator.vs");

using namespace sw;

bool gPrintDependencies;
bool gPrintOverriddenDependencies;
bool gOutputNoConfigSubdir;

int vsVersionFromString(const String &s)
{
    String t;
    for (auto c : s)
    {
        if (isdigit(c))
            t += c;
    }
    if (t.empty())
        return 0;
    auto v = std::stoi(t);
    if (t.size() == 4)
    {
        switch (v)
        {
            // 2003
        case 2005:
            return 8;
        case 2008:
            return 9;
        case 2010:
            return 10;
        case 2012:
            return 11;
        case 2013:
            return 12;
        case 2015:
            return 14;
        case 2017:
            return 15;
        case 2019:
            return 16;
        }
    }
    else if (t.size() == 2)
    {
        return v;
    }
    throw SW_RUNTIME_ERROR("Unknown or bad VS version: " + t);
}

static String uuid2string(const boost::uuids::uuid &u)
{
    std::ostringstream ss;
    ss << u;
    return boost::to_upper_copy(ss.str());
}

static path get_int_dir(const path &dir, const path &projects_dir, const String &name)
{
    auto tdir = dir / projects_dir;
    return tdir / "i" / shorten_hash(blake2b_512(name), 6);
}

static path get_int_dir(const path &dir, const path &projects_dir, const String &name, const BuildSettings &s)
{
    return get_int_dir(dir, projects_dir, name) / shorten_hash(blake2b_512(get_project_configuration(s)), 6);
}

static path get_out_dir(const path &dir, const path &projects_dir, const BuildSettings &s)
{
    auto p = fs::current_path();
    p /= "bin";
    if (!gOutputNoConfigSubdir)
        p /= get_configuration(s);
    return p;
}

enum class VSFileType
{
    None,
    ResourceCompile,
    CustomBuild,
    ClInclude,
    ClCompile,
    MASM,
    Manifest,
};

static String toString(VSFileType t)
{
    switch (t)
    {
    case VSFileType::ClCompile:
        return "ClCompile";
    case VSFileType::ClInclude:
        return "ClInclude";
    case VSFileType::ResourceCompile:
        return "ResourceCompile";
    case VSFileType::CustomBuild:
        return "CustomBuild";
    case VSFileType::MASM:
        return "MASM";
    case VSFileType::Manifest:
        return "Manifest";
    default:
        return "None";
    }
}

static VSFileType get_vs_file_type_by_ext(const path &p)
{
    if (p.extension() == ".rc")
        return VSFileType::ResourceCompile;
    else if (p.extension() == ".rule")
        return VSFileType::CustomBuild;
    else if (isCppHeaderFileExtension(p.extension().string()))
        return VSFileType::ClInclude;
    else if (isCppSourceFileExtensions(p.extension().string()) || p.extension() == ".c")
        return VSFileType::ClCompile;
    else if (p.extension() == ".asm")
        return VSFileType::MASM;
    else if (p.extension() == ".manifest")
        return VSFileType::Manifest;
    return VSFileType::None;
}

void VSGenerator::generate(const SwBuild &b)
{
    const sw::InsecurePath deps_subdir = "Dependencies";
    const sw::InsecurePath overridden_deps_subdir = "Overridden Packages";
    const String predefined_targets_dir = ". SW Predefined Targets"s;
    const String visualizers_dir = "Visualizers"s;
    const String all_build_name = "ALL_BUILD"s;
    const String build_dependencies_name = "BUILD_DEPENDENCIES"s;

    version = Version(16);
    sln_root = b.getBuildDirectory() / toPathString(getType()) / version.toString(1);

    Solution s;

    auto inputs = b.getInputs();
    if (inputs.size() != 1)
        throw SW_RUNTIME_ERROR("unsupported");
    for (auto &i : inputs)
        s.settings = i.getSettings();

    Directory d(predefined_targets_dir);
    d.g = this;
    s.directories[d.name] = d;

    Project p(all_build_name);
    p.g = this;
    p.directory = predefined_targets_dir;
    for (auto &i : inputs)
    {
        if (i.getInput().getType() == InputType::DirectorySpecificationFile)
            p.files.insert(i.getInput().getPath());
    }
    p.settings = s.settings;
    s.projects[p.name] = p;

    for (auto &[pkg, tgts] : b.getTargetsToBuild())
    {
        for (auto &tgt : tgts)
        {
            Project p(pkg.toString());
            p.g = this;
            p.files = tgt->getSourceFiles();
            p.settings = s.settings;
            p.files = tgt->getSourceFiles();
            s.projects[p.name] = p;
            break;
        }
    }
    for (auto &[pkg, tgts] : b.getTargetsToBuild())
    {
        for (auto &tgt : tgts)
        {
            auto deps = tgt->getDependencies();
            for (auto &d : deps)
            {
                // filter out predefined & deps targets
                auto &pd = b.getTargetsToBuild();
                if (pd.find(d->getUnresolvedPackage().ppath) == pd.end(d->getUnresolvedPackage().ppath))
                    continue;
                s.projects[tgt->getPackage().toString()].dependencies.insert(&s.projects[d->getTarget().getPackage().toString()]);
            }
            break;
        }
    }
    s.emit(*this);
}

void Solution::emit(const VSGenerator &g) const
{
    SolutionEmitter ctx;
    ctx.version = g.version;
    ctx.printVersion();
    emitDirectories(ctx);
    emitProjects(g.sln_root, ctx);

    ctx.beginGlobal();
    ctx.setSolutionConfigurationPlatforms(*this);
    //
    ctx.beginGlobalSection("ProjectConfigurationPlatforms", "postSolution");
    for (auto &[n, p] : projects)
    {
        ctx.addProjectConfigurationPlatforms(p, g.getType() == GeneratorType::VisualStudio);
        //if (projects.find(p.toString() + "-build") != projects.end())
            //addProjectConfigurationPlatforms(b, p.toString() + "-build");
    }
    // we do not need it here
    if (g.getType() != GeneratorType::VisualStudio)
    {
        SW_UNIMPLEMENTED;
        //ctx.addProjectConfigurationPlatforms(b, all_build_name, true);
    }
    ctx.endGlobalSection();
    //
    ctx.beginGlobalSection("NestedProjects", "preSolution");
    for (auto &[n, p] : projects)
    {
        if (p.directory.empty())
            continue;
        ctx.addKeyValue(p.uuid, directories.find(p.directory)->second.uuid);
    }
    ctx.endGlobalSection();
    ctx.endGlobal();

    //const auto compiler_name = boost::to_lower_copy(toString(b.solutions[0].Settings.Native.CompilerType));
    const String compiler_name = "msvc";
    //String fn = b.ide_solution_name + "_";
    String fn = "p_";
    fn += compiler_name + "_" + toPathString(g.getType()) + "_" + g.version.toString(1);
    fn += ".sln";
    write_file_if_different(g.sln_root / fn, ctx.getText());
    auto lnk = current_thread_path() / fn;
    lnk += ".lnk";
    ::create_link(g.sln_root / fn, lnk, "SW link");

    for (auto &[n, p] : projects)
        p.emit(g);
}

void Solution::emitDirectories(SolutionEmitter &ctx) const
{
    for (auto &[n, d] : directories)
    {
        ctx.addDirectory(d);
    }
}

void Solution::emitProjects(const path &root, SolutionEmitter &sctx) const
{
    for (auto &[n, p] : projects)
    {
        p.emit(sctx);
    }
}

Directory::Directory(const String &name)
    : name(name)
{
    auto up = boost::uuids::name_generator_sha1(boost::uuids::ns::oid())(name);
    uuid = "{" + uuid2string(up) + "}";
}

Project::Project(const String &name)
    : Directory(name)
{
    type = VSProjectType::Utility;
}

void Project::emit(SolutionEmitter &ctx) const
{
    ctx.beginProject(*this);
    if (!dependencies.empty())
    {
        ctx.beginProjectSection("ProjectDependencies", "postProject");
        for (auto &d : dependencies)
            ctx.addLine(d->uuid + " = " + d->uuid);
        ctx.endProjectSection();
    }
    ctx.endProject();
}

void Project::emit(const VSGenerator &g) const
{
    emitProject(g);
    emitFilters(g);
}

void Project::emitProject(const VSGenerator &g) const
{
    ProjectEmitter ctx;
    ctx.beginProject(g.version);
    ctx.addProjectConfigurations(*this);

    ctx.beginBlock("PropertyGroup", {{"Label", "Globals"}});
    ctx.addBlock("VCProjectVersion", std::to_string(g.version.getMajor()) + ".0");
    ctx.addBlock("ProjectGuid", uuid);
    ctx.addBlock("Keyword", "Win32Proj");
    if (g.getType() != GeneratorType::VisualStudio)
        ctx.addBlock("ProjectName", name);
    else
    {
        ctx.addBlock("RootNamespace", name);
        //pctx.addBlock("WindowsTargetPlatformVersion", ctx.getSettings().begin()->Native.SDK.getWindowsTargetPlatformVersion());
    }
    ctx.addBlock("PreferredToolArchitecture", "x64"); // also x86
    ctx.endBlock();

    ctx.addBlock("Import", "", {{"Project", "$(VCTargetsPath)\\Microsoft.Cpp.Default.props"}});
    ctx.addPropertyGroupConfigurationTypes(*this);
    ctx.addBlock("Import", "", {{"Project", "$(VCTargetsPath)\\Microsoft.Cpp.props"}});
    ctx.addPropertySheets(*this);

    // make conditional if .asm files are present
    ctx.beginBlock("ImportGroup", {{"Label", "ExtensionSettings"}});
    ctx.addBlock("Import", "", {{"Project", "$(VCTargetsPath)\\BuildCustomizations\\masm.props"}});
    ctx.endBlock();
    ctx.beginBlock("ImportGroup", {{"Label", "ExtensionTargets"}});
    ctx.addBlock("Import", "", {{"Project", "$(VCTargetsPath)\\BuildCustomizations\\masm.targets"}});
    ctx.endBlock();

    ctx.beginBlock("ItemGroup");
    //pctx.addBlock(toString(get_vs_file_type_by_ext(*b.config)), {{"Include", b.config->u8string()}});
    ctx.endBlock();

    auto get_int_dir = [&g, this](auto &s)
    {
        return ::get_int_dir(g.sln_root, vs_project_dir, name, s);
    };

    for (auto &s : settings)
    {
        ctx.beginBlockWithConfiguration("PropertyGroup", s);
        {
            ctx.addBlock("OutDir", normalize_path_windows(get_out_dir(g.sln_root, vs_project_dir, s)) + "\\");
            ctx.addBlock("IntDir", normalize_path_windows(get_int_dir(s)) + "\\int\\");
            // full name of target, keep as is (it might have subdirs)
            ctx.addBlock("TargetName", name);
            //addBlock("TargetExt", ext);
        }
        ctx.endBlock();
    }

    ctx.beginBlock("ItemGroup");
    for (auto &p : files)
    {
        auto t = get_vs_file_type_by_ext(p);
        ctx.beginBlock(toString(t), { { "Include", p.u8string() } });
        //add_obj_file(t, p, sf);
        /*if (sf->skip)
        {
        beginBlock("ExcludedFromBuild");
        addText("true");
        endBlock(true);
        }*/
        ctx.endBlock();
    }
    ctx.endBlock();

    ctx.addBlock("Import", "", {{"Project", "$(VCTargetsPath)\\Microsoft.Cpp.targets"}});

    ctx.endProject();
    write_file(g.sln_root / vs_project_dir / name += vs_project_ext, ctx.getText());
}

void Project::emitFilters(const VSGenerator &g) const
{
    return;

    FiltersEmitter ctx;
    ctx.beginProject();
    ctx.beginBlock("ItemGroup");
    ctx.endBlock();
    ctx.endProject();
    write_file(g.sln_root / vs_project_dir / name += vs_project_ext += ".filters", ctx.getText());
}
