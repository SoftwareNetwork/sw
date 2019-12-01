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

#include <boost/algorithm/string.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <nlohmann/json.hpp>
#include <primitives/http.h>
#ifdef _WIN32
#include <primitives/win32helpers.h>
#endif

#include <sstream>
#include <stack>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "generator.vs");

using namespace sw;

bool gPrintDependencies;
bool gPrintOverriddenDependencies;
bool gOutputNoConfigSubdir;

static FlagTables flag_tables;

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

static String make_backslashes(String s)
{
    std::replace(s.begin(), s.end(), '/', '\\');
    return s;
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

static FlagTable read_flag_table(const path &fn)
{
    auto j = nlohmann::json::parse(read_file(fn));
    FlagTable ft;
    for (auto &flag : j)
    {
        FlagTableData d;
        d.name = flag["name"].get<String>();
        if (d.name.empty())
            continue;
        d.argument = flag["switch"].get<String>();
        d.comment = flag["comment"].get<String>();
        d.value = flag["value"].get<String>();
        //d.flags = flag["name"].get<String>();
        //ft.table[d.name] = d;
        for (auto &f : flag["flags"])
        {
            if (f == "UserValue")
                d.flags |= FlagTableFlags::UserValue;
            else if (f == "SemicolonAppendable")
                d.flags |= FlagTableFlags::SemicolonAppendable;
            else if (f == "UserRequired")
                d.flags |= FlagTableFlags::UserRequired;
            else if (f == "UserIgnored")
                d.flags |= FlagTableFlags::UserIgnored;
            else if (f == "UserFollowing")
                d.flags |= FlagTableFlags::UserFollowing;
            else if (f == "Continue")
                d.flags |= FlagTableFlags::Continue;
            else if (f == "CaseInsensitive")
                d.flags |= FlagTableFlags::CaseInsensitive;
            else if (f == "SpaceAppendable")
                d.flags |= FlagTableFlags::SpaceAppendable;
            else
                LOG_WARN(logger, "Unknown flag: " + f.get<String>());
        }
        ft.ftable[d.argument] = d;
    }
    return ft;
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

    // dl flag tables from cmake
    static const String ft_base_url = "https://gitlab.kitware.com/cmake/cmake/raw/master/Templates/MSBuild/FlagTables/";
    static const String ft_ext = ".json";
    const Strings tables1 = { "CL", "Link" };
    const Strings tables2 = { "LIB", "MASM", "RC" };
    auto ts = getVsToolset(version);
    auto dl = [](const auto &ts, const auto &tbl)
    {
        for (auto &t : tbl)
        {
            auto fn = ts + "_" + t + ".json";
            auto url = ft_base_url + fn;
            auto out = get_root_directory() / "FlagTables" / fn;
            if (!fs::exists(out))
                download_file(url, out);
            auto ft = read_flag_table(out);
            auto prog = boost::to_lower_copy(t);
            if (prog == "masm")
            {
                flag_tables["ml"] = ft;
                flag_tables["ml64"] = ft;
            }
            else
            {
                flag_tables[prog] = ft;
            }
        }
    };
    dl(ts, tables1);
    dl(ts.substr(0, ts.size() - 1), tables2);

    Solution s;

    auto inputs = b.getInputs();
    if (inputs.size() != 1)
        throw SW_RUNTIME_ERROR("unsupported");
    auto &input = *inputs.begin();
    s.settings = input.getSettings();

    // get settings from targets to make use settings equality later
    for (auto &[pkg, tgts] : b.getTargetsToBuild())
    {
        decltype(s.settings) s2;
        for (auto &st : s.settings)
        {
            auto itgt = tgts.findSuitable(st);
            if (itgt == tgts.end())
                throw SW_RUNTIME_ERROR("missing target");
            s2.insert((*itgt)->getSettings());
        }
        if (s2.size() != s.settings.size())
            throw SW_RUNTIME_ERROR("settings size do not match");
        s.settings = s2;
        break;
    }

    {
        Directory d(predefined_targets_dir);
        d.g = this;
        s.directories.emplace(d.name, d);
    }

    {
        Project p(all_build_name);
        p.g = this;
        p.directory = predefined_targets_dir;
        if (input.getInput().getType() == InputType::SpecificationFile ||
            input.getInput().getType() == InputType::InlineSpecification)
            p.files.insert(input.getInput().getPath());
        p.settings = s.settings;
        // create datas
        for (auto &st : s.settings)
            p.getData(st).type = p.type;
        s.projects.emplace(p.name, p);
    }

    for (auto &[pkg, tgts] : b.getTargetsToBuild())
    {
        for (auto &tgt : tgts)
        {
            Project p(pkg.toString());
            p.g = this;
            p.files = tgt->getSourceFiles();
            p.settings = s.settings;
            p.build = true;

            s.projects.emplace(p.name, p);
            s.projects.find(all_build_name)->second.dependencies.insert(&s.projects.find(p.name)->second);
            break;
        }
        for (auto &st : s.settings)
        {
            auto itgt = tgts.findEqual(st);
            if (itgt == tgts.end())
                throw SW_RUNTIME_ERROR("missing target");
            auto &d = s.projects.find(pkg.toString())->second.getData(st);
            d.target = itgt->get();

            // determine type
            auto cmds = d.target->getCommands();
            bool has_dll = false;
            bool has_exe = false;
            for (auto &c : cmds)
            {
                has_dll |= std::any_of(c->outputs.begin(), c->outputs.end(), [&d, &c](const auto &f)
                {
                    bool r = f.extension() == ".dll";
                    if (r)
                        d.main_command = c.get();
                    return r;
                });
                has_exe |= std::any_of(c->outputs.begin(), c->outputs.end(), [&d, &c](const auto &f)
                {
                    bool r = f.extension() == ".exe";
                    if (r)
                        d.main_command = c.get();
                    return r;
                });
            }
            if (has_exe)
                d.type = VSProjectType::Application;
            else if (has_dll)
                d.type = VSProjectType::DynamicLibrary;
            else
            {
                d.type = VSProjectType::StaticLibrary;
                for (auto &c : cmds)
                {
                    for (auto &f : c->outputs)
                    {
                        if (f.extension() == ".lib")
                        {
                            d.main_command = c.get();
                            break;
                        }
                    }
                }
            }
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
                s.projects.find(tgt->getPackage().toString())->second.dependencies.insert(&s.projects.find(d->getTarget().getPackage().toString())->second);
            }
            break;
        }
    }

    // gather .natvis
    Files natvis;
    for (auto &[n, p] : s.projects)
    {
        for (auto &f : p.files)
        {
            if (f.extension() == ".natvis")
                natvis.insert(f);
        }
    }

    if (!natvis.empty())
    {
        Directory d(visualizers_dir);
        d.g = this;
        d.files = natvis;
        d.directory = predefined_targets_dir;
        s.directories.emplace(d.name, d);
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
    for (auto &[n, p] : directories)
    {
        if (p.directory.empty())
            continue;
        ctx.addKeyValue(p.uuid, directories.find(p.directory)->second.uuid);
    }
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
#ifdef _WIN32
    ::create_link(g.sln_root / fn, lnk, "SW link");
#endif

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

ProjectData &Project::getData(const sw::TargetSettings &s)
{
    return data[s];
}

const ProjectData &Project::getData(const sw::TargetSettings &s) const
{
    auto itd = data.find(s);
    if (itd == data.end())
        throw SW_RUNTIME_ERROR("no such settings");
    return itd->second;
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

    static const StringSet skip_cl_props =
    {
        "ShowIncludes",
        "ObjectFileName",
        "SuppressStartupBanner",
    };

    static const StringSet skip_link_props =
    {
        "ImportLibrary",
        "OutputFile",
        "ProgramDatabaseFile",
        "ImportLibrary",
        "SuppressStartupBanner",
    };

    Properties link_props;
    link_props.exclude_flags = skip_link_props;
    link_props.exclude_exts = {".obj", ".res"};

    Properties cl_props;
    cl_props.exclude_flags = skip_cl_props;

    for (auto &s : settings)
    {
        auto &d = getData(s);
        ctx.beginBlockWithConfiguration("ItemDefinitionGroup", s);
        {
            ctx.beginBlock("Link");
            if (d.main_command)
                printProperties(ctx, *d.main_command, link_props);
            ctx.endBlock();
        }
        ctx.endBlock();
    }

    ctx.beginBlock("ItemGroup");
    for (auto &p : files)
    {
        if (p.extension() == ".natvis")
            continue;

        auto t = get_vs_file_type_by_ext(p);
        ctx.beginBlock(toString(t), { { "Include", p.u8string() } });

        for (auto &[s, d] : data)
        {
            if (!d.target)
                continue;
            auto cmds = d.target->getCommands();
            auto itcmd = std::find_if(cmds.begin(), cmds.end(), [&p](const auto &c)
            {
                return std::any_of(c->inputs.begin(), c->inputs.end(), [&p](const auto &f)
                {
                    return p == f;
                });
            });
            if (itcmd != cmds.end())
            {
                auto c = *itcmd;
                printProperties(ctx, *c, cl_props);
                //ctx.beginBlockWithConfiguration("AdditionalOptions", s);
                //ctx.endBlock();
            }
            else
                LOG_WARN(logger, "File " << p << " is not processed");
        }

        //add_obj_file(t, p, sf);
        /*if (!build)
        {
            ctx.beginBlock("ExcludedFromBuild");
            ctx.addText("true");
            ctx.endBlock(true);
        }*/
        ctx.endBlock();
    }
    ctx.endBlock();

    ctx.addBlock("Import", "", {{"Project", "$(VCTargetsPath)\\Microsoft.Cpp.targets"}});

    ctx.endProject();
    write_file_if_different(g.sln_root / vs_project_dir / name += vs_project_ext, ctx.getText());
}

void Project::emitFilters(const VSGenerator &g) const
{
    StringSet filters; // dirs

    String sd, bd, bdp;
    std::set<path> parents;
    for (auto &f : files)
        parents.insert(f.parent_path());
    // take shortest
    if (!parents.empty())
        sd = normalize_path(*parents.begin());

    FiltersEmitter ctx;
    ctx.beginProject();

    ctx.beginBlock("ItemGroup");
    for (auto &f : files)
    {
        if (f.extension() == ".natvis")
            continue;

        String *d = nullptr;
        size_t p = 0;
        auto fd = normalize_path(f);

        auto calc = [&fd, &p, &d](auto &s)
        {
            if (s.empty())
                return;
            auto p1 = fd.find(s);
            if (p1 != 0)
                return;
            //if (p1 > p)
            {
                p = s.size();
                d = &s;
            }
        };

        calc(sd);
        calc(bd);
        calc(bdp);

        path filter;
        if (p != -1)
        {
            auto ss = fd.substr(p);
            if (ss[0] == '/')
                ss = ss.substr(1);
            path r = ss;
            if (d == &sd)
                r = "Source Files" / r;
            /*if (d == &bd)
            {
                auto v = r;
                r = "Generated Files";
                r /= get_configuration(s);
                r /= "Public" / v;
            }
            if (d == &bdp)
            {
                auto v = r;
                r = "Generated Files";
                r /= get_configuration(s);
                r /= "Private" / v;
            }*/
            do
            {
                r = r.parent_path();
                if (filter.empty())
                    filter = r;
                filters.insert(r.string());
            } while (!r.empty() && r != r.root_path());
        }

        ctx.beginBlock(toString(get_vs_file_type_by_ext(f)), { {"Include", f.string()} });
        if (!filter.empty() && !filter.is_absolute())
            ctx.addBlock("Filter", make_backslashes(filter.string()));
        ctx.endBlock();
    }
    filters.erase("");
    ctx.endBlock();

    ctx.beginBlock("ItemGroup");
    for (auto &f : filters)
    {
        ctx.beginBlock("Filter", { { "Include", make_backslashes(f) } });
        ctx.addBlock("UniqueIdentifier", "{" + uuid2string(boost::uuids::name_generator_sha1(boost::uuids::ns::oid())(make_backslashes(f))) + "}");
        ctx.endBlock();
    }
    ctx.endBlock();

    ctx.endProject();
    write_file(g.sln_root / vs_project_dir / (name + vs_project_ext + ".filters"), ctx.getText());
}

void Project::printProperties(ProjectEmitter &ctx, const primitives::Command &c, const Properties &props) const
{
    auto ft = path(c.getProgram()).stem().u8string();
    auto ift = flag_tables.find(ft);
    if (ift == flag_tables.end())
    {
        // we must create custom rule here
        LOG_TRACE(logger, "No flag table: " + ft);
        return;
    }

    std::map<String, String> semicolon_args;
    bool skip_prog = false;
    for (auto &o : c.arguments)
    {
        if (!skip_prog)
        {
            skip_prog = true;
            continue;
        }

        auto arg = o->toString();

        if (!arg.empty() && arg[0] != '-' && arg[0] != '/')
        {
            if (props.exclude_exts.find(path(arg).extension().string()) != props.exclude_exts.end())
                continue;
            semicolon_args["AdditionalDependencies"] += arg + ";";
            continue;
        }

        auto &tbl = flag_tables[ft].ftable;

        auto print = [&ctx, &arg, &semicolon_args, &props](auto &d)
        {
            if (props.exclude_flags.find(d.name) != props.exclude_flags.end())
                return;
            if (bitmask_includes(d.flags, FlagTableFlags::UserValue))
            {
                if (bitmask_includes(d.flags, FlagTableFlags::SemicolonAppendable))
                {
                    semicolon_args[d.name] += arg.substr(1 + d.argument.size()) + ";";
                    return;
                }
                else
                {
                    ctx.beginBlock(d.name);
                    ctx.addText(arg.substr(1 + d.argument.size()));
                }
            }
            else
            {
                ctx.beginBlock(d.name);
                ctx.addText(d.value);
            }
            ctx.endBlock(true);
        };

        // fast lookup first
        auto i = tbl.find(arg.substr(1));
        if (i != tbl.end())
        {
            print(i->second);
            continue;
        }

        // TODO: we must find the longest match
        //bool found = false;
        for (auto &[_, d] : tbl)
        {
            if (d.argument.empty())
                continue;
            if (arg.find(d.argument, 1) != 1)
                continue;
            print(d);
            //found = true;
            break;
        }
        //if (!found)
            //LOG_WARN(logger, "arg not found: " + arg);
    }
    for (auto &[k, v] : semicolon_args)
    {
        ctx.beginBlock(k);
        ctx.addText(v);
        ctx.endBlock(true);
    }
}
