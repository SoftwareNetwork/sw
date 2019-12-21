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

#include <sw/builder/file.h>
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
static const auto SourceFilesFilter = "Source Files";

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

static Version clver2vsver(const Version &clver, const Version &clmaxver)
{
    if (clver >= Version(19, 20))
    {
        return Version(16);
    }

    if (clver >= Version(19, 10) && clver < Version(19, 20))
    {
        // vs 16 (v142) can also handle v141 toolset.
        if (clmaxver >= Version(19, 20))
            return Version(16);
        return Version(15);
    }

    if (clver >= Version(19, 00) && clver < Version(19, 10))
    {
        return Version(14);
    }

    LOG_WARN(logger, "Untested branch");
    return Version(13); // ?
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

void VSGenerator::generate(const SwBuild &b)
{
    const sw::InsecurePath deps_subdir = "Dependencies";
    const sw::InsecurePath overridden_deps_subdir = "Overridden Packages";
    const String predefined_targets_dir = ". SW Predefined Targets"s;
    const String visualizers_dir = "Visualizers"s;
    const String all_build_name = "ALL_BUILD"s;
    const String build_dependencies_name = "BUILD_DEPENDENCIES"s;

    auto inputs = b.getInputs();
    if (inputs.size() != 1)
        throw SW_RUNTIME_ERROR("unsupported number of inputs, must be 1");
    auto &input = *inputs.begin();

    PackagePathTree path_tree;
    Solution s;
    s.settings = input.getSettings();

    UnresolvedPackage compiler = (*s.settings.begin())["native"]["program"]["cpp"].getValue();
    auto compiler_id = b.getContext().getPredefinedTargets().find(compiler)->first;
    auto compiler_id_max = b.getContext().getPredefinedTargets().find(UnresolvedPackage(compiler.getPath().toString()))->first;

    vs_version = clver2vsver(compiler_id.getVersion(), compiler_id_max.getVersion());
    toolset_version = compiler_id.getVersion();
    // this removes hash part      vvvvvvvvvvvvv
    sln_root = getRootDirectory(b).parent_path() / vs_version.toString(1);

    // dl flag tables from cmake
    static const String ft_base_url = "https://gitlab.kitware.com/cmake/cmake/raw/master/Templates/MSBuild/FlagTables/";
    static const String ft_ext = ".json";
    const Strings tables1 = { "CL", "Link" };
    const Strings tables2 = { "LIB", "MASM", "RC" };
    auto ts = getVsToolset(toolset_version);
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

    // get settings from targets to use settings equality later
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

    // add predefined dirs
    {
        Directory d(predefined_targets_dir);
        d.g = this;
        s.directories.emplace(d.name, d);
    }

    // add ALL_BUILD project
    {
        Project p(all_build_name);
        p.g = this;
        p.directory = &s.directories.find(predefined_targets_dir)->second;
        if (input.getInput().getType() == InputType::SpecificationFile ||
            input.getInput().getType() == InputType::InlineSpecification)
        {
            p.files.insert({ input.getInput().getPath(), SourceFilesFilter });
        }
        p.settings = s.settings;
        // create datas
        for (auto &st : s.settings)
            p.getData(st).type = p.type;
        s.projects.emplace(p.name, p);
    }

    auto can_add_file = [](const auto &f)
    {
        auto t = get_vs_file_type_by_ext(f);
        return t == VSFileType::ClInclude || t == VSFileType::None;
    };

    auto is_generated_ext = [](const auto &f)
    {
        return
            0
            || f.extension() == ".obj"
            || f.extension() == ".lib"
            || f.extension() == ".dll"
            || f.extension() == ".exe"
            || f.extension() == ".res"
            || f.extension() == ".pdb"
            // add more
            ;
    };

    for (auto &[pkg, tgts] : b.getTargetsToBuild())
    {
        // add project with settings
        for (auto &tgt : tgts)
        {
            Project p(pkg.toString());
            p.g = this;
            for (auto &f : tgt->getSourceFiles())
            {
                if (can_add_file(f))
                    p.files.insert(f);
            }
            p.settings = s.settings;
            p.build = true;
            p.source_dir = tgt->getInterfaceSettings()["source_dir"].getValue();

            s.projects.emplace(p.name, p);
            s.projects.find(all_build_name)->second.dependencies.insert(&s.projects.find(p.name)->second);
            break;
        }

        // process project
        auto &p = s.projects.find(pkg.toString())->second;
        for (auto &st : s.settings)
        {
            auto itgt = tgts.findEqual(st);
            if (itgt == tgts.end())
                throw SW_RUNTIME_ERROR("missing target");
            auto &d = s.projects.find(pkg.toString())->second.getData(st);
            d.target = itgt->get();
            path_tree.add(d.target->getPackage());

            d.binary_dir = d.target->getInterfaceSettings()["binary_dir"].getValue();
            d.binary_private_dir = d.target->getInterfaceSettings()["binary_private_dir"].getValue();

            auto cmds = d.target->getCommands();

            bool has_dll = false;
            bool has_exe = false;
            for (auto &c : cmds)
            {
                for (auto &o : c->inputs)
                {
                    if (is_generated_ext(o))
                        continue;

                    if (can_add_file(o))
                        p.files.insert(o);
                    else
                        d.build_rules[c.get()] = o;
                }

                for (auto &o : c->outputs)
                {
                    if (is_generated_ext(o))
                        continue;

                    if (can_add_file(o))
                        p.files.insert(o);

                    if (1
                        && c->arguments.size() > 1
                        && c->arguments[1]->toString() == sw::builder::getInternalCallBuiltinFunctionName()
                        && c->arguments.size() > 3
                        && c->arguments[3]->toString() == "sw_create_def_file"
                        )
                    {
                        d.pre_link_command = c.get();
                        continue;
                    }

                    d.custom_rules.insert(c.get());
                }

                // determine project type and main command
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

            d.build_rules.erase(d.main_command);
        }
    }
    for (auto &[pkg, tgts] : b.getTargetsToBuild())
    {
        for (auto &tgt : tgts)
        {
            auto &p = s.projects.find(tgt->getPackage().toString())->second;
            auto &data = p.getData(tgt->getSettings());
            auto deps = tgt->getDependencies();
            for (auto &d : deps)
            {
                // filter out predefined targets
                auto &pd1 = b.getContext().getPredefinedTargets();
                if (pd1.find(d->getUnresolvedPackage().ppath) != pd1.end(d->getUnresolvedPackage().ppath))
                    continue;

                // filter out predefined & deps targets
                auto &pd = b.getTargetsToBuild();
                if (pd.find(d->getUnresolvedPackage().ppath) == pd.end(d->getUnresolvedPackage().ppath))
                {
                    data.dependencies.insert(&d->getTarget());
                    continue;
                }
                p.dependencies.insert(&s.projects.find(d->getTarget().getPackage().toString())->second);
            }
        }
    }

    // natvis
    {
        // gather .natvis
        FilesWithFilter natvis;
        for (auto &[n, p] : s.projects)
        {
            for (auto &f : p.files)
            {
                if (f.p.extension() == ".natvis")
                    natvis.insert(f);
            }
        }

        if (!natvis.empty())
        {
            Directory d(visualizers_dir);
            d.g = this;
            d.files = natvis;
            d.directory = &s.directories.find(predefined_targets_dir)->second;
            s.directories.emplace(d.name, d);
        }
    }

    // add BUILD_DEPENDENCIES project
    {
        {
            Project p(build_dependencies_name);
            p.g = this;
            p.directory = &s.directories.find(predefined_targets_dir)->second;
            p.settings = s.settings;
            s.projects.emplace(p.name, p);
        }

        auto &p = s.projects.find(build_dependencies_name)->second;

        // create datas
        for (auto &st : s.settings)
            p.getData(st).type = p.type;
        for (auto &st : s.settings)
        {
            auto &d = p.getData(st);

            auto int_dir = get_int_dir(sln_root, vs_project_dir, p.name, st);

            // fake command
            Rule r;
            r.name = p.name;
            r.command = "setlocal";
            r.outputs.insert(int_dir / "rules" / "intentionally_missing.file");
            r.verify_inputs_and_outputs_exist = false;

            d.custom_rules_manual.push_back(r);

            // actually we must build deps + their specific settings
            // not one setting for all deps
            std::map<PackageId, String> deps;
            for (auto &[_, p1] : s.projects)
            {
                auto &d = p1.getData(st);
                for (auto &t : d.dependencies)
                {
                    deps[t->getPackage()] = t->getSettings().toString();
                    p1.dependencies.insert(&p); // add dependency for project
                }
            }

            String deps_str;
            for (auto &[d,s] : deps)
                deps_str += d.toString() + " " + s + " ";
            auto fn = shorten_hash(blake2b_512(deps_str), 6);
            auto basefn = int_dir / fn;

            auto fix_json = [](auto s)
            {
                boost::replace_all(s, "\\", "\\\\");
                boost::replace_all(s, "\"", "\\\"");
                return "\"" + s + "\"";
            };

            Strings args;
            args.push_back("-d");
            args.push_back(normalize_path(fs::current_path()));
            args.push_back("build");
            args.push_back("-input-settings-pairs");
            for (auto &[d, s] : deps)
            {
                args.push_back(d.toString());
                args.push_back(fix_json(s));
            }
            args.push_back("-ide-fast-path");
            args.push_back(normalize_path(path(basefn) += ".deps"));
            args.push_back("-ide-copy-to-dir");
            args.push_back(normalize_path(b.getBuildDirectory() / "out" / st.getHash()));

            String s;
            for (auto &a : args)
                s += a + "\n";
            auto rsp = path(basefn) += ".rsp";
            write_file(rsp, s);

            BuildEvent be;
            be.command = "sw @" + normalize_path(rsp);
            d.pre_build_event = be;
        }
    }

    // add path dirs
    {
        auto parents = path_tree.getDirectories();
        for (auto &p : parents)
        {
            auto pp = p.parent();
            while (!pp.empty() && parents.find(pp) == parents.end())
                pp = pp.parent();

            Directory d(p);
            d.visible_name = p.slice(pp.size());
            d.g = this;
            if (!pp.empty())
                d.directory = &s.directories.find(pp.toString())->second;
            s.directories.emplace(d.name, d);
        }

        // set project dirs
        for (auto &[pkg, tgts] : b.getTargetsToBuild())
        {
            for (auto &tgt : tgts)
            {
                auto &p = s.projects.find(tgt->getPackage().toString())->second;
                auto pp = tgt->getPackage().getPath();
                while (!pp.empty() && parents.find(pp) == parents.end())
                    pp = pp.parent();
                // sometimes there a project and a dir with same name
                // in this case select parent dir
                if (pp == tgt->getPackage().getPath())
                {
                    pp = pp.parent();
                    while (!pp.empty() && parents.find(pp) == parents.end())
                        pp = pp.parent();
                }
                if (!pp.empty())
                {
                    p.directory = &s.directories.find(pp.toString())->second;
                    p.visible_name = sw::PackageId(tgt->getPackage().getPath().slice(pp.size()), tgt->getPackage().getVersion()).toString();
                }
                break;
            }
        }
    }

    // main emit
    s.emit(*this);
}

void Solution::emit(const VSGenerator &g) const
{
    SolutionEmitter ctx;
    ctx.version = g.vs_version;
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
        if (!p.directory)
            continue;
        ctx.addKeyValue(p.uuid, p.directory->uuid);
    }
    for (auto &[n, p] : projects)
    {
        if (!p.directory)
            continue;
        ctx.addKeyValue(p.uuid, p.directory->uuid);
    }
    ctx.endGlobalSection();
    ctx.endGlobal();

    //const auto compiler_name = boost::to_lower_copy(toString(b.solutions[0].Settings.Native.CompilerType));
    const String compiler_name = "msvc";
    String fn = fs::current_path().filename().u8string() + "_";
    fn += compiler_name + "_" + toPathString(g.getType()) + "_" + g.vs_version.toString(1);
    fn += ".sln";
    auto visible_lnk_name = fn;
    write_file_if_different(g.sln_root / fn, ctx.getText());

    // write bat for multiprocess compilation
    if (g.vs_version >= Version(16))
    {
        String bat;
        bat += "@echo off\n";
        bat += "setlocal\n";
        bat += ":: turn on multiprocess compilation\n";
        bat += "set UseMultiToolTask=true\n";
        bat += "start " + normalize_path_windows(g.sln_root / fn) + "\n";
        fn += ".bat"; // we now make a link to bat file
        write_file_if_different(g.sln_root / fn, bat);
    }

    // link
    auto lnk = fs::current_path() / visible_lnk_name;
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

CommonProjectData::CommonProjectData(const String &name)
    : name(name)
{
    auto up = boost::uuids::name_generator_sha1(boost::uuids::ns::oid())(name);
    uuid = "{" + uuid2string(up) + "}";
}

String CommonProjectData::getVisibleName() const
{
    if (visible_name.empty())
        return name;
    return visible_name;
}

Project::Project(const String &name)
    : CommonProjectData(name)
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
    ctx.beginProject(g.vs_version);
    ctx.addProjectConfigurations(*this);

    ctx.beginBlock("PropertyGroup", {{"Label", "Globals"}});
    ctx.addBlock("VCProjectVersion", std::to_string(g.vs_version.getMajor()) + ".0");
    ctx.addBlock("ProjectGuid", uuid);
    ctx.addBlock("Keyword", "Win32Proj");
    if (g.getType() == GeneratorType::VisualStudio)
    {
        ctx.addBlock("RootNamespace", getVisibleName());
        ctx.addBlock("WindowsTargetPlatformVersion", PackageId((*settings.begin())["native"]["stdlib"]["c"].getValue()).getVersion().toString());
    }
    ctx.addBlock("ProjectName", getVisibleName());
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
        auto &d = getData(s);
        ctx.beginBlockWithConfiguration("PropertyGroup", s);
        {
            if (d.main_command)
                ctx.addBlock("OutDir", normalize_path_windows(d.main_command->outputs.begin()->parent_path()) + "\\");
            //else
                //ctx.addBlock("OutDir", normalize_path_windows(get_out_dir(g.sln_root, vs_project_dir, s)) + "\\");
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
        "SuppressStartupBanner",

        // When we turn this on, we must provide this property for object files with some cpp names
        // but in different directory.
        // Otherwise in VS pre 16 (pre VS2019) there's no way to perform multiprocess compilation,
        // when this is turned off.
        //"ObjectFileName",
    };

    static const StringSet skip_link_props =
    {
        //"ImportLibrary",
        //"OutputFile",
        //"ProgramDatabaseFile",
        "SuppressStartupBanner",
    };

    Properties link_props;
    link_props.exclude_flags = skip_link_props;
    link_props.exclude_exts = {".obj", ".res"};

    Properties cl_props;
    cl_props.exclude_flags = skip_cl_props;

    for (auto &s : settings)
    {
        auto commands_dir = get_int_dir(s) / "commands";

        auto &d = getData(s);
        ctx.beginBlockWithConfiguration("ItemDefinitionGroup", s);
        {
            //
            if (d.main_command)
            {
                ctx.beginBlock(d.type == VSProjectType::StaticLibrary ? "Lib" : "Link");
                printProperties(ctx, s, *d.main_command, link_props);
                ctx.endBlock();
            }

            if (d.pre_link_command)
            {
                auto cmd = d.pre_link_command->writeCommand(commands_dir / std::to_string(d.pre_link_command->getHash()));

                ctx.beginBlock("PreLinkEvent");
                ctx.beginBlock("Command");
                ctx.addText("call \"" + normalize_path_windows(cmd) + "\"");
                ctx.endBlock(true);
                ctx.endBlock();
            }

            //
            ctx.beginBlock("ClCompile");

            ctx.beginBlock("MultiProcessorCompilation");
            ctx.addText("true");
            ctx.endBlock(true);

            ctx.endBlock();

            if (d.pre_build_event)
            {
                ctx.beginBlock("PreBuildEvent");

                ctx.beginBlock("Command");
                ctx.addText(d.pre_build_event->command);
                ctx.endBlock(true);

                ctx.endBlock();
            }
        }
        ctx.endBlock();
    }

    ctx.beginBlock("ItemGroup");

    // usual files
    for (auto &p : files)
    {
        if (p.p.extension() == ".natvis")
            continue;

        ctx.beginFileBlock(p.p);

        /*for (auto &[s, d] : data)
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
            //else
                //LOG_WARN(logger, "File " << p << " is not processed");
        }*/

        //add_obj_file(t, p, sf);
        //if (!build)
        //{
            //ctx.beginBlock("ExcludedFromBuild");
            //ctx.addText("true");
            //ctx.endBlock(true);
        //}
        ctx.endFileBlock();
    }

    // build rules
    std::map<path, std::map<const sw::TargetSettings *, Command>> bfiles;
    for (auto &[s, d] : data)
    {
        for (auto &[c, f] : d.build_rules)
        {
            bfiles[f][&s] = c;
        }
    }
    for (auto &[f, cfgs] : bfiles)
    {
        ((Project&)*this).files.insert(f);
        auto t = ctx.beginFileBlock(f);
        for (auto &[sp, c] : cfgs)
        {
            printProperties(ctx, *sp, *c, cl_props);

            // one .rc file
            //if (t == VSFileType::ResourceCompile)
            if (sw::File(f, c->getContext().getFileStorage()).isGeneratedAtAll())
            {
                for (auto &[s, d] : data)
                {
                    if (sp == &s)
                        continue;
                    ctx.beginBlockWithConfiguration("ExcludedFromBuild", s);
                    ctx.addText("true");
                    ctx.endBlock(true);
                }
            }
        }
        ctx.endFileBlock();
    }

    // custom rules
    for (auto &[s, d] : data)
    {
        auto int_dir = get_int_dir(s);
        auto rules_dir = get_int_dir(s) / "rules";
        auto commands_dir = get_int_dir(s) / "commands";

        Files rules;
        for (auto &c : d.custom_rules)
        {
            // TODO: add hash if two rules with same name
            path rule = rules_dir / c->outputs.begin()->filename();
            rules.insert(rule);
            if (rules.find(rule) != rules.end())
                rule += "." + std::to_string(c->getHash());
            rule += ".rule";
            write_file(rule, "");
            ((Project&)*this).files.insert({rule, ". SW Rules"});

            auto cmd = c->writeCommand(commands_dir / std::to_string(c->getHash()));

            ctx.beginFileBlock(rule);

            ctx.beginBlockWithConfiguration("AdditionalInputs", s);
            for (auto &o : c->inputs)
                ctx.addText(normalize_path_windows(o) + ";");
            ctx.endBlock(true);

            ctx.beginBlockWithConfiguration("Outputs", s);
            for (auto &o : c->outputs)
                ctx.addText(normalize_path_windows(o) + ";");
            ctx.endBlock(true);

            ctx.beginBlockWithConfiguration("Command", s);
            ctx.addText("call \"" + normalize_path_windows(cmd) + "\"");
            ctx.endBlock(true);

            ctx.beginBlockWithConfiguration("BuildInParallel", s);
            ctx.addText("true");
            ctx.endBlock(true);

            ctx.beginBlockWithConfiguration("Message", s);
            ctx.endBlock();

            for (auto &[s1, d] : data)
            {
                if (s == s1)
                    continue;
                ctx.beginBlockWithConfiguration("ExcludedFromBuild", s1);
                ctx.addText("true");
                ctx.endBlock(true);
            }

            ctx.endFileBlock();
        }

        for (auto &c : d.custom_rules_manual)
        {
            path rule = rules_dir / c.name;
            rule += ".rule";
            write_file(rule, "");
            ((Project&)*this).files.insert({rule, ". SW Rules"});

            ctx.beginFileBlock(rule);

            ctx.beginBlockWithConfiguration("Outputs", s);
            for (auto &o : c.outputs)
                ctx.addText(normalize_path_windows(o) + ";");
            ctx.endBlock(true);

            ctx.beginBlockWithConfiguration("Command", s);
            ctx.addText(c.command);
            ctx.endBlock(true);

            ctx.beginBlockWithConfiguration("Message", s);
            ctx.endBlock();

            if (g.vs_version >= Version(16))
            {
                if (!c.verify_inputs_and_outputs_exist)
                {
                    ctx.beginBlockWithConfiguration("VerifyInputsAndOutputsExist", s);
                    ctx.addText("false");
                    ctx.endBlock(true);
                }
            }

            for (auto &[s1, d] : data)
            {
                if (&s1 == &s)
                    continue;
                ctx.beginBlockWithConfiguration("ExcludedFromBuild", s1);
                ctx.addText("true");
                ctx.endBlock(true);
            }

            ctx.endFileBlock();
        }
    }
    ctx.endBlock();

    ctx.addBlock("Import", "", {{"Project", "$(VCTargetsPath)\\Microsoft.Cpp.targets"}});

    ctx.endProject();
    write_file_if_different(g.sln_root / vs_project_dir / name += vs_project_ext, ctx.getText());
}

void Project::emitFilters(const VSGenerator &g) const
{
    StringSet filters; // dirs

    String sd = normalize_path(source_dir);

    FiltersEmitter ctx;
    ctx.beginProject();

    ctx.beginBlock("ItemGroup");
    for (auto &f : files)
    {
        if (f.p.extension() == ".natvis")
            continue;

        if (!f.filter.empty())
        {
            filters.insert(make_backslashes(f.filter.string()));
            ctx.beginBlock(toString(get_vs_file_type_by_ext(f.p)), { {"Include", f.p.string()} });
            ctx.addBlock("Filter", make_backslashes(f.filter.string()));
            ctx.endBlock();
            continue;
        }

        String *d = nullptr;
        const sw::TargetSettings *s = nullptr; // also mark generated files
        bool bdir_private = false;
        size_t p = 0;
        auto fd = normalize_path(f.p);

        auto calc = [&fd, &p, &d](auto &s)
        {
            if (s.empty())
                return;
            auto p1 = fd.find(s);
            if (p1 != 0)
                return;
            //if (p1 > p)
            //if (p1 != -1 && p1 > p)
            {
                p = s.size();
                d = &s;
            }
        };

        calc(sd);

        for (const auto &d1 : data)
        {
            String bd = normalize_path(d1.second.binary_dir);
            String bdp = normalize_path(d1.second.binary_private_dir);

            calc(bd);
            calc(bdp);

            if (d == &bdp)
            {
                s = &d1.first;
                bdir_private = true;
                break;
            }

            if (d == &bd)
            {
                s = &d1.first;
                break;
            }
        }

        path filter;
        if (p != -1)
        {
            auto ss = fd.substr(p);
            if (ss[0] == '/')
                ss = ss.substr(1);
            path r = ss;

            if (d == &sd)
                r = SourceFilesFilter / r;

            if (s)
            {
                if (!bdir_private)
                {
                    auto v = r;
                    r = "Generated Files";
                    r /= (*s)["os"]["arch"].getValue();
                    r /= get_configuration(*s);
                    r /= "Public" / v;
                }
                else
                {
                    auto v = r;
                    r = "Generated Files";
                    r /= (*s)["os"]["arch"].getValue();
                    r /= get_configuration(*s);
                    r /= "Private" / v;
                }
            }

            do
            {
                r = r.parent_path();
                if (filter.empty())
                    filter = r;
                filters.insert(r.string());
            } while (!r.empty() && r != r.root_path());
        }

        ctx.beginBlock(toString(get_vs_file_type_by_ext(f.p)), { {"Include", f.p.string()} });
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

void Project::printProperties(ProjectEmitter &ctx, const sw::TargetSettings &s, const sw::builder::Command &c, const Properties &props) const
{
    for (auto &d : c.getGeneratedDirs())
        fs::create_directories(d);

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
    for (int na = 0; na < c.arguments.size(); na++)
    {
        if (!skip_prog)
        {
            skip_prog = true;
            continue;
        }

        auto &o = c.arguments[na];
        auto arg = o->toString();

        auto add_additional_args = [&semicolon_args, &ft, &c, &props](const auto &arg)
        {
            if (props.exclude_exts.find(path(arg).extension().string()) != props.exclude_exts.end())
                return;
            if (ft == "cl")
            {
                if (arg == "-c" || arg == "-FS")
                    return;
                auto i = c.inputs.find(normalize_path(arg));
                if (i != c.inputs.end())
                    return;
                semicolon_args["AdditionalOptions"] += arg + " ";
                return;
            }
            semicolon_args["AdditionalDependencies"] += arg + ";";
        };

        if (!arg.empty() && arg[0] != '-' && arg[0] != '/')
        {
            add_additional_args(arg);
            continue;
        }

        auto &tbl = flag_tables[ft].ftable;

        auto print = [&ctx, &arg, &semicolon_args, &props, &s, &c, &na, &ft](auto &d)
        {
            if (props.exclude_flags.find(d.name) != props.exclude_flags.end())
                return;
            if (bitmask_includes(d.flags, FlagTableFlags::UserValue))
            {
                auto a = arg.substr(1 + d.argument.size());

                // if we get empty string, probably value is in the next arg
                if (a.empty())
                    a = c.arguments[++na]->toString().substr(1 + d.argument.size());

                // filters
                if (ft == "rc" && arg.find("-D") == 0)
                {
                    // fix quotes for -D in .rc files
                    boost::replace_all(a, "\"", "\\\"");
                }

                if (bitmask_includes(d.flags, FlagTableFlags::SemicolonAppendable))
                {
                    semicolon_args[d.name] += a + ";";
                    return;
                }
                else
                {
                    ctx.beginBlockWithConfiguration(d.name, s);
                    ctx.addText(a);
                }
            }
            else
            {
                ctx.beginBlockWithConfiguration(d.name, s);
                ctx.addText(d.value);
            }
            ctx.endBlock(true);
        };

        if (arg.empty())
        {
            LOG_WARN(logger, "Empty arg for command: " + c.print());
            continue;
        }

        // fast lookup first
        auto i = tbl.find(arg.substr(1));
        if (i != tbl.end())
        {
            print(i->second);
            continue;
        }

        // TODO: we must find the longest match
        bool found = false;
        for (auto &[_, d] : tbl)
        {
            if (d.argument.empty())
                continue;
            if (arg.find(d.argument, 1) != 1)
                continue;
            print(d);
            found = true;
            break;
        }
        if (!found)
        {
            //LOG_WARN(logger, "arg not found: " + arg);

            add_additional_args(arg);
            continue;
        }
    }
    //if (ft == "cl")
        //semicolon_args["AdditionalOptions"] += "%(AdditionalOptions)";
    for (auto &[k, v] : semicolon_args)
    {
        ctx.beginBlockWithConfiguration(k, s);
        ctx.addText(v);
        ctx.endBlock(true);
    }
}

void PackagePathTree::add(const sw::PackageId &p)
{
    add(p.getPath(), p);
}

void PackagePathTree::add(const sw::PackagePath &p, const sw::PackageId &project)
{
    if (p.empty())
    {
        projects.insert(project);
        return;
    }
    tree[p.slice(0, 1).toString()].add(p.slice(1), project);
}

PackagePathTree::Directories PackagePathTree::getDirectories(const sw::PackagePath &p)
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
