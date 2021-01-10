// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>

#include "vs.h"

#include "../generator.h"
#include "project_emitter.h"
#include "solution_emitter.h"

#include <sw/builder/file.h>
#include <sw/builder/execution_plan.h>
#include <sw/core/build.h>
#include <sw/core/input.h>
#include <sw/core/specification.h>
#include <sw/core/sw_context.h>
#include <sw/driver/build_settings.h>
#include <sw/manager/storage.h>
#include <sw/support/filesystem.h>

#include <boost/algorithm/string.hpp>
#include <boost/dll.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <nlohmann/json.hpp>
#include <primitives/http.h>
#include <primitives/sw/cl.h>
#ifdef _WIN32
#include <primitives/win32helpers.h>
#endif

#include <sstream>
#include <stack>

#ifdef _WIN32
#include <shellapi.h>
#endif

#include <cl.llvm.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "generator.vs");

// TODO: add TESTS target (or how it is named in cmake)

using namespace sw;

static FlagTables flag_tables;
static const auto SourceFilesFilter = "Source Files";

extern String vs_zero_check_stamp_ext;

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
        /*
        //VS7 = 71,
        VS8 = 80,
        VS9 = 90,
        VS10 = 100,
        VS11 = 110,
        VS12 = 120,
        //VS13 = 130 was skipped
        VS14 = 140,
        VS15 = 150,
        VS16 = 160,
        */
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

static auto fix_json(String s)
{
    boost::replace_all(s, "\\", "\\\\");
    boost::replace_all(s, "\"", "\\\"");
    return "\"" + s + "\"";
};

static sw::PackageVersion clver2vsver(const sw::PackageVersion &clver, const sw::PackageVersion &clmaxver)
{
    if (clver >= sw::PackageVersion::Version(19, 20))
    {
        return sw::PackageVersion::Version(16);
    }

    if (clver >= sw::PackageVersion::Version(19, 10) && clver < sw::PackageVersion::Version(19, 20))
    {
        // vs 16 (v142) can also handle v141 toolset.
        if (clmaxver >= sw::PackageVersion::Version(19, 20))
            return sw::PackageVersion::Version(16);
        return sw::PackageVersion::Version(15);
    }

    if (clver >= sw::PackageVersion::Version(19, 00) && clver < sw::PackageVersion::Version(19, 10))
    {
        return sw::PackageVersion::Version(14);
    }

    LOG_WARN(logger, "Untested branch");
    return sw::PackageVersion::Version(13); // ?
}

static String uuid2string(const boost::uuids::uuid &u)
{
    std::ostringstream ss;
    ss << u;
    return boost::to_upper_copy(ss.str());
}

static String get_current_program()
{
    return "\"" + to_string(normalize_path(path(boost::dll::program_location().wstring()))) + "\"";
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

static path get_out_dir(const path &dir, const path &projects_dir, const BuildSettings &s, const Options &options)
{
    auto p = fs::current_path();
    p /= "bin";
    if (!options.options_generate.output_no_config_subdir)
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

bool is_generated_ext(const path &f)
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

void VSGenerator::generate(const SwBuild &b)
{
    const String predefined_targets_dir = ". SW Predefined Targets"s;
    const String visualizers_dir = "Visualizers"s;
    const String all_build_name = "ALL_BUILD"s;
    const String build_dependencies_name = "BUILD_DEPENDENCIES"s;
    const String zero_check_name = "ZERO_CHECK"s;
    this->b = &b;

    auto inputs = b.getInputs();
    PackagePathTree path_tree;
    Solution s;

    // gather ttb and settings
    TargetMap ttb;
    SW_UNIMPLEMENTED;
    /*for (auto &[pkg, tgts] : b.getTargetsToBuild())
    {
        auto add = [&ttb, &pkg = pkg, &tgts = tgts, &s]()
        {
            ttb[pkg] = tgts;
            for (auto &tgt : tgts)
                s.settings.insert(tgt->getSettings());
        };

        if (add_all_packages)
        {
            add();
            continue;
        }

        if (add_overridden_packages)
        {
            sw::LocalPackage p(b.getContext().getLocalStorage(), pkg);
            if (p.isOverridden())
            {
                add();
                continue;
            }
        }

        if (pkg.getPath().isAbsolute())
            continue;

        if (tgts.empty())
            throw SW_RUNTIME_ERROR("empty target");
        add();
    }*/

    if (s.settings.empty())
        throw SW_RUNTIME_ERROR("Empty settings");

    auto compiler_type_s = (*s.settings.begin())["rule"]["cpp"]["type"].getValue();
    if (compiler_type_s == "msvc")
        ;
    else if (compiler_type_s == "clangcl")
        compiler_type = ClangCl;
    else if (compiler_type_s == "clang")
    {
        compiler_type = Clang;
        LOG_INFO(logger, "Not yet fully supported");
    }
    else
        throw SW_RUNTIME_ERROR("Compiler is not supported (yet?): " + compiler_type_s);

    UnresolvedPackageName compiler = (*s.settings.begin())["rule"]["cpp"]["package"].getValue();
    auto compiler_id = b.getTargets().find(compiler)->first;
    auto compiler_id_max_version = b.getTargets().find(UnresolvedPackageName(compiler.getPath().toString()))->first;

    if (compiler_type == MSVC)
    {
        vs_version = clver2vsver(compiler_id.getVersion(), compiler_id_max_version.getVersion());
        toolset_version = compiler_id.getVersion();
    }
    else
    {
        // otherwise just generate maximum found version for msvc compiler
        auto compiler_id_max_version = b.getTargets().find(UnresolvedPackageName("com.Microsoft.VisualStudio.VC.cl"))->first;
        vs_version = clver2vsver(compiler_id_max_version.getVersion(), compiler_id_max_version.getVersion());
        toolset_version = compiler_id_max_version.getVersion();
    }
    // this removes hash part      vvvvvvvvvvvvv
    sln_root = getRootDirectory(b).parent_path() / vs_version.getVersion().toString(1);

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
            auto out = support::get_root_directory() / "FlagTables" / fn;
            if (!fs::exists(out))
                download_file(url, out);
            auto ft = read_flag_table(out);
            auto prog = boost::to_lower_copy(t);
            if (prog == "masm")
            {
                flag_tables["ml"] = ft;
                //flag_tables["ml64"] = ft;
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
    for (auto &[pkg, tgts] : ttb)
    {
        decltype(s.settings) s2;
        for (auto &st : s.settings)
        {
            auto itgt = tgts.findSuitable(st);
            if (itgt == tgts.end())
                throw SW_RUNTIME_ERROR("missing target: " + pkg.toString() + ", settings: " + st.toString());
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

    // add ZERO_CHECK project
    {
        Project p(zero_check_name);
        p.g = this;
        p.directory = &s.directories.find(predefined_targets_dir)->second;
        p.settings = s.settings;
        // create datas
        for (auto &st : s.settings)
            p.getData(st).type = p.type;

        for (auto &[s, d] : p.data)
        {
            Rule r;
            r.name = "generate.stamp";
            r.message = "Checking Build System";
            r.command += "setlocal\r\n";
            r.command += "cd \"" + to_string(normalize_path_windows(fs::current_path())) + "\"\r\n";
            d.custom_rules_manual.push_back(r);
        }

        s.projects.emplace(p.name, p);
    }

    // add ALL_BUILD project
    {
        Project p(all_build_name);
        p.g = this;
        p.directory = &s.directories.find(predefined_targets_dir)->second;
        for (auto &i : inputs)
        {
            for (auto &[_, f] : i.getInput().getSpecification().files.getData())
                p.files.insert({f.absolute_path, SourceFilesFilter});
        }
        p.settings = s.settings;
        if (vstype != VsGeneratorType::VisualStudio)
            p.type = VSProjectType::Makefile;
        // create datas
        for (auto &st : s.settings)
            p.getData(st).type = p.type;
        p.dependencies.insert(&s.projects.find(zero_check_name)->second);
        if (vstype != VsGeneratorType::VisualStudio)
        {
            // save explan
            //b.saveExecutionPlan();
            // we must split configs or something like that

            for (auto &st : s.settings)
            {
                auto &d = p.getData(st);

                String cmd;
                cmd = "-d " + to_string(normalize_path(fs::current_path())) + " build -input-settings-pairs ";
                for (auto &i : inputs)
                {
                    for (auto &[_, f] : i.getInput().getSpecification().files.getData())
                    {
                        cmd += "\"" + to_string(normalize_path(f.absolute_path)) + "\" ";
                        cmd += fix_json(st.toString()) + " ";
                    }
                }

                // TODO: switch to swexplans
                // sw -config d build -e
                // sw -config d build -ef .sw\g\swexplan\....explan

                d.nmake_build = get_current_program() + " " + cmd;
                d.nmake_rebuild = get_current_program() + " -B " + cmd;
                //d.nmake_clean = "sw "; // not yet implemented
            }
        }

        // register
        s.projects.emplace(p.name, p);
    }

    auto can_add_file = [](const auto &f)
    {
        auto t = get_vs_file_type_by_ext(f);
        return t == VSFileType::ClInclude || t == VSFileType::None;
    };

    int n_executables = 0;

    // write basic config files
    std::unordered_map<sw::PackageSettings, Files> configure_files;
    for (auto &i : inputs)
    {
        for (auto &[_, f] : i.getInput().getSpecification().files.getData())
        {
            for (auto &st : s.settings)
                configure_files[st].insert(f.absolute_path);
        }
    }

    for (auto &[pkg, tgts] : ttb)
    {
        // add project with settings
        for (auto &tgt : tgts)
        {
            Project p(pkg.toString());
            p.g = this;
            for (auto &[f,tf] : tgt->getFiles(
                //StorageFileType::SourceArchive
            ))
            {
                if (tf.isGenerated() && f.extension() != ".natvis")
                    continue;
                if (can_add_file(f))
                    p.files.insert(f);
            }
            p.settings = s.settings;
            p.build = true;
            p.source_dir = tgt->getInterfaceSettings()["source_dir"].getValue();

            p.dependencies.insert(&s.projects.find(zero_check_name)->second);

            s.projects.emplace(p.name, p);
            s.projects.find(all_build_name)->second.dependencies.insert(&s.projects.find(p.name)->second);

            // some other stuff
            n_executables += tgt->getInterfaceSettings()["type"] == "native_executable"s;
            if (!s.first_project && tgt->getInterfaceSettings()["ide"]["startup_project"])
                s.first_project = &s.projects.find(p.name)->second;
            break;
        }

        // process project
        auto &p = s.projects.find(pkg.toString())->second;
        for (auto &st : s.settings)
        {
            auto itgt = tgts.findEqual(st);
            if (itgt == tgts.end())
                throw SW_RUNTIME_ERROR("missing target: " + pkg.toString());
            auto &d = s.projects.find(pkg.toString())->second.getData(st);
            SW_UNIMPLEMENTED;
            //d.target = itgt->get();
            path_tree.add(d.target->getPackage());

            d.binary_dir = d.target->getInterfaceSettings()["binary_dir"].getValue();
            d.binary_private_dir = d.target->getInterfaceSettings()["binary_private_dir"].getValue();

            auto cfs = d.target->getInterfaceSettings()["ide"]["configure_files"].getArray();
            for (auto &cf : cfs)
                configure_files[d.target->getSettings()].insert(cf.getPathValue(b.getContext().getLocalStorage()));

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
            if (vstype != VsGeneratorType::VisualStudio)
                d.type = VSProjectType::Utility;

            d.build_rules.erase(d.main_command);
        }
    }
    for (auto &[pkg, tgts] : ttb)
    {
        for (auto &tgt : tgts)
        {
            auto &p = s.projects.find(tgt->getPackage().toString())->second;
            auto &data = p.getData(tgt->getSettings());
            auto &is = tgt->getInterfaceSettings();

            auto add_deps = [&ttb, &data, &s, &b, &p](auto &is)
            {
                for (auto &[id, v] : is)
                {
                    PackageName d(id);
                    // filter out predefined targets
                    if (b.isPredefinedTarget(d))
                        continue;

                    // filter out NON TARGET TO BUILD deps
                    // add them to just deps list
                    auto &pd = ttb;
                    if (pd.find(d) == pd.end())
                    {
                        SW_UNIMPLEMENTED;
                        /*auto i = b.getTargets().find(d, v.getMap());
                        if (!i)
                            throw SW_LOGIC_ERROR("Cannot find dependency: " + d.toString());
                        data.dependencies.insert(i);
                        continue;*/
                    }
                    p.dependencies.insert(&s.projects.find(d.toString())->second);
                }
            };

            add_deps(is["dependencies"]["link"].getMap());
            add_deps(is["dependencies"]["dummy"].getMap());

            //
            if (!s.first_project && n_executables == 1 && tgt->getInterfaceSettings()["type"] == "native_executable"s)
                s.first_project = &p;
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

    // ZERO_BUILD rule
    {
        auto &p = s.projects.find(zero_check_name)->second;
        for (auto &[st, cfs] : configure_files)
        {
            auto &d = p.getData(st);
            auto int_dir = get_int_dir(sln_root, vs_project_dir, p.name, st);
            path fn = int_dir / "check_list.txt";
            auto stampfn = path(fn) += vs_zero_check_stamp_ext;

            auto &r = d.custom_rules_manual.back();

            //
            r.command += get_current_program() + " ";
            r.command += "generate -check-stamp-list \"" + to_string(normalize_path(fn)) + "\" ";
            r.command += "-input-settings-pairs ";
            for (auto &i : inputs)
            {
                for (auto &s : i.getSettings())
                {
                    for (auto &[_, f] : i.getInput().getSpecification().files.getData())
                    {
                        r.command += "\"" + to_string(normalize_path(f.absolute_path)) + "\" ";
                        r.command += fix_json(s.toString()) + " ";
                    }
                }
            }
            r.outputs.insert(stampfn);
            r.inputs = cfs;

            String s;
            uint64_t mtime = 0;
            for (auto &f : cfs)
            {
                s += to_string(normalize_path(f)) + "\n";

                if (!fs::exists(f))
                    throw SW_RUNTIME_ERROR("Input file does not exist: " + to_string(normalize_path(s)));
                auto lwt = fs::last_write_time(f);
                mtime ^= file_time_type2time_t(lwt);
            }
            write_file(fn, s);
            write_file(stampfn, std::to_string(mtime));
        }
    }

    // add BUILD_DEPENDENCIES project
    if (vstype == VsGeneratorType::VisualStudio)
    {
        {
            Project p(build_dependencies_name);
            p.g = this;
            p.directory = &s.directories.find(predefined_targets_dir)->second;
            p.settings = s.settings;
            p.dependencies.insert(&s.projects.find(zero_check_name)->second);
            s.projects.emplace(p.name, p);
        }

        auto &p = s.projects.find(build_dependencies_name)->second;

        // create datas
        for (auto &st : s.settings)
            p.getData(st).type = p.type;

        bool has_deps = false;
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
            std::map<PackageName, String> deps;
            for (auto &[_, p1] : s.projects)
            {
                auto &d = p1.getData(st);
                for (auto &t : d.dependencies)
                {
                    deps[t->getPackage()] = t->getSettings().toString();
                    p1.dependencies.insert(&p); // add dependency for project
                }
            }
            if (deps.empty())
                continue;
            has_deps = true;

            String deps_str;
            for (auto &[d,s] : deps)
                deps_str += d.toString() + " " + s + " ";
            auto fn = shorten_hash(blake2b_512(deps_str), 6);
            auto basefn = int_dir / fn;

            Strings args;
            args.push_back("-d");
            args.push_back(to_string(normalize_path(fs::current_path())));
            args.push_back("build");
            args.push_back("-input-settings-pairs");
            for (auto &[d, s] : deps)
            {
                args.push_back(d.toString());
                args.push_back(fix_json(s));
            }
            args.push_back("-ide-fast-path");
            args.push_back(to_string(normalize_path(path(basefn) += ".deps")));
            args.push_back("-ide-copy-to-dir");
            if (st["name"])
                args.push_back(to_string(normalize_path(b.getBuildDirectory() / "out" / st["name"].getValue())));
            else
                args.push_back(to_string(normalize_path(b.getBuildDirectory() / "out" / st.getHashString())));

            String s;
            for (auto &a : args)
                s += a + "\n";
            auto rsp = path(basefn) += ".rsp";
            write_file(rsp, s);

            error_code ec;
            fs::remove(path(basefn) += ".deps", ec); // trigger updates

            BuildEvent be;
            be.command = get_current_program() + " @" + to_string(normalize_path(rsp));
            d.pre_build_event = be;
        }

        if (!has_deps)
            s.projects.erase(build_dependencies_name);
    }

    // add path dirs
    {
        auto parents = path_tree.getDirectories();
        for (auto &p : parents)
        {
            auto pp = p.parent();
            while (!pp.empty() && parents.find(pp) == parents.end())
                pp = pp.parent();

            Directory d(p.toString());
            d.visible_name = p.slice(pp.size()).toString();
            d.g = this;
            if (!pp.empty())
                d.directory = &s.directories.find(pp.toString())->second;
            s.directories.emplace(d.name, d);
        }

        // set project dirs
        for (auto &[pkg, tgts] : ttb)
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
                    p.visible_name = sw::PackageName(tgt->getPackage().getPath().slice(pp.size()), tgt->getPackage().getVersion()).toString();
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

    if (first_project)
        first_project->emit(ctx);
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
    String fn = to_string(fs::current_path().filename().u8string()) + "_";
    fn += compiler_name + "_" + g.getPathString().string() + "_" + g.vs_version.getVersion().toString(1);
    fn += ".sln";
    auto visible_lnk_name = fn;
    write_file_if_different(g.sln_root / fn, ctx.getText());

    // write bat for multiprocess compilation
    if (g.vs_version >= sw::PackageVersion::Version(16))
    {
        String bat;
        bat += "@echo off\n";
        bat += "setlocal\n";
        bat += ":: turn on multiprocess compilation\n";
        bat += "set UseMultiToolTask=true\n";
        //bat += "set EnforceProcessCountAcrossBuilds=true\n";
        bat += "start " + to_string(normalize_path_windows(g.sln_root / fn)) + "\n";
        // for preview cl versions run preview VS later
        // start "c:\Program Files (x86)\Microsoft Visual Studio\2019\Preview\Common7\IDE\devenv.exe" fn
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
        if (first_project == &p)
            continue;
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

ProjectData &Project::getData(const sw::PackageSettings &s)
{
    return data[s];
}

const ProjectData &Project::getData(const sw::PackageSettings &s) const
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

    ProjectEmitter ctx;
    ctx.beginProject(g.vs_version);
    ctx.addProjectConfigurations(*this);

    ctx.beginBlock("PropertyGroup", {{"Label", "Globals"}});
    ctx.addBlock("VCProjectVersion", std::to_string(g.vs_version.getMajor()) + ".0");
    ctx.addBlock("ProjectGuid", uuid);
    ctx.addBlock("Keyword", "Win32Proj");
    if (g.vstype == VsGeneratorType::VisualStudio)
    {
        UnresolvedPackageName ucrt = (*settings.begin())["native"]["stdlib"]["c"].getValue();
        auto ucrt_id = g.b->getTargets().find(ucrt)->first;

        ctx.addBlock("RootNamespace", getVisibleName());
        ctx.addBlock("WindowsTargetPlatformVersion", ucrt_id.getVersion().toString());
        //ctx.addBlock("WindowsTargetPlatformVersion", PackageId((*settings.begin())["native"]["stdlib"]["c"].getValue()).getVersion().toString());
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

    // build files
    std::map<path, std::map<const sw::PackageSettings *, Command>> bfiles;
    std::unordered_map<sw::PackageSettings, std::map<String /*ft*/, std::map<String /*opt*/, String /*val*/>>> common_cl_options;
    for (auto &[s, d] : data)
    {
        std::map<String /*opt*/, std::map<std::pair<String, String> /*command line argument*/, int /*count*/>> cl_opts;
        std::map<String /*opt*/, int /*count*/> ft_count;
        for (auto &[c, f] : d.build_rules)
        {
            // gather opts
            auto ft = get_flag_table(*c, false);
            if (ft.empty())
            {
                LOG_TRACE(logger, "No flag table for file: " + to_string(normalize_path(f)));
                continue;
            }

            // without flag table, we do not add file
            bfiles[f][&s] = c;

            ft_count[ft]++;
            for (auto &v : printProperties(*c, cl_props))
                cl_opts[ft][v]++;
        }

        // gather common opts
        auto &cl_opts2 = common_cl_options[s];
        for (auto &[ft, v1] : cl_opts)
        {
            for (auto &[k, v] : v1)
            {
                if (v == ft_count[ft])
                    cl_opts2[ft][k.first] = k.second;
            }
        }
    }

    //
    for (auto &s : settings)
    {
        auto &d = getData(s);
        ctx.beginBlockWithConfiguration("PropertyGroup", s);
        {
            if (d.main_command)
                ctx.addBlock("OutDir", to_string(normalize_path_windows(d.main_command->outputs.begin()->parent_path())) + "\\");
            //else
                //ctx.addBlock("OutDir", normalize_path_windows(get_out_dir(g.sln_root, vs_project_dir, s)) + "\\");
            ctx.addBlock("IntDir", to_string(normalize_path_windows(get_int_dir(s))) + "\\int\\");
            // full name of target, keep as is (it might have subdirs)
            ctx.addBlock("TargetName", name);
            //addBlock("TargetExt", ext);

            if (!d.nmake_build.empty())
                ctx.addBlock("NMakeBuildCommandLine", d.nmake_build);
            if (!d.nmake_clean.empty())
                ctx.addBlock("NMakeCleanCommandLine", d.nmake_clean);
            if (!d.nmake_rebuild.empty())
                ctx.addBlock("NMakeReBuildCommandLine", d.nmake_rebuild);
        }
        ctx.endBlock();
    }

    //
    StringSet used_flag_tables;
    for (auto &s : settings)
    {
        auto commands_dir = get_int_dir(s) / "commands";

        auto &d = getData(s);
        ctx.beginBlockWithConfiguration("ItemDefinitionGroup", s);
        {
            //
            if (d.main_command)
            {
                for (auto &d : d.main_command->getGeneratedDirs())
                    fs::create_directories(d);

                ctx.beginBlock(d.type == VSProjectType::StaticLibrary ? "Lib" : "Link");
                for (auto &[k, v] : printProperties(*d.main_command, link_props))
                {
                    ctx.beginBlockWithConfiguration(k, s);
                    ctx.addText(v);
                    ctx.endBlock(true);
                }
                ctx.endBlock();
            }

            if (d.pre_link_command)
            {
                for (auto &d : d.pre_link_command->getGeneratedDirs())
                    fs::create_directories(d);

                auto cmd = d.pre_link_command->writeCommand(commands_dir / std::to_string(d.pre_link_command->getHash()));

                ctx.beginBlock("PreLinkEvent");
                ctx.beginBlock("Command");
                ctx.addText("call \"" + to_string(normalize_path_windows(cmd)) + "\"");
                ctx.endBlock(true);
                ctx.endBlock();
            }

            // ClCompile
            {
                ctx.beginBlock("ClCompile");

                //if (g.compiler_type != VSGenerator::Clang)
                {
                    ctx.beginBlock("MultiProcessorCompilation");
                    ctx.addText("true");
                    ctx.endBlock(true);
                }

                // common opts
                for (auto &[k, v] : common_cl_options[s]["cl"])
                {
                    ctx.beginBlockWithConfiguration(k, s);
                    ctx.addText(v);
                    if (g.compiler_type == VSGenerator::ClangCl && k == "AdditionalOptions")
                        ctx.addText("-showFilenames ");
                    ctx.endBlock(true);
                }
                used_flag_tables.insert("cl");

                for (auto &[k, v] : common_cl_options[s]["clang"])
                {
                    ctx.beginBlockWithConfiguration(k, s);
                    ctx.addText(v);
                    ctx.endBlock(true);
                }
                used_flag_tables.insert("clang");

                ctx.endBlock();
            }

            // ResourceCompile
            {
                ctx.beginBlock("ResourceCompile");
                // common opts
                for (auto &[k, v] : common_cl_options[s]["rc"])
                {
                    ctx.beginBlockWithConfiguration(k, s);
                    ctx.addText(v);
                    ctx.endBlock(true);
                }
                used_flag_tables.insert("rc");
                ctx.endBlock();
            }

            {
                ctx.beginBlock("MASM");
                // common opts
                for (auto &[k, v] : common_cl_options[s]["ml"])
                {
                    ctx.beginBlockWithConfiguration(k, s);
                    ctx.addText(v);
                    ctx.endBlock(true);
                }
                used_flag_tables.insert("ml");
                ctx.endBlock();
            }

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
        ctx.endFileBlock();
    }

    // build rules
    for (auto &[f, cfgs] : bfiles)
    {
        ((Project&)*this).files.insert(f);
        auto t = ctx.beginFileBlock(f);
        for (auto &[sp, c] : cfgs)
        {
            for (auto &d : c->getGeneratedDirs())
                fs::create_directories(d);

            auto ft = get_flag_table(*c);
            if (used_flag_tables.find(ft) == used_flag_tables.end())
                throw SW_RUNTIME_ERROR("Flag table was not set: " + ft);
            auto &cl_opts = common_cl_options[*sp][ft];
            for (auto &[k, v] : printProperties(*c, cl_props))
            {
                if (cl_opts.find(k) != cl_opts.end())
                    continue;
                ctx.beginBlockWithConfiguration(k, *sp);
                ctx.addText(v);
                ctx.endBlock(true);
            }

            // one .rc file
            if (t == VSFileType::ResourceCompile || sw::File(f, c->getContext().getFileStorage()).isGenerated())
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

        if (d.type != VSProjectType::Utility)
        {
            Files rules;
            for (auto &c : d.custom_rules)
            {
                for (auto &d : c->getGeneratedDirs())
                    fs::create_directories(d);

                // TODO: add hash if two rules with same name
                path rule = rules_dir / c->outputs.begin()->filename();
                rules.insert(rule);
                if (rules.find(rule) != rules.end())
                    rule += "." + std::to_string(c->getHash());
                rule += ".rule";
                if (!fs::exists(rule)) // prevent rebuilds
                    write_file(rule, "");
                ((Project &)*this).files.insert({ rule, ". SW Rules" });

                auto cmd = c->writeCommand(commands_dir / std::to_string(c->getHash()), false);

                ctx.beginFileBlock(rule);

                ctx.beginBlockWithConfiguration("AdditionalInputs", s);
                for (auto &o : c->inputs)
                    ctx.addText(to_string(normalize_path_windows(o)) + ";");
                ctx.endBlock(true);

                ctx.beginBlockWithConfiguration("Outputs", s);
                for (auto &o : c->outputs)
                    ctx.addText(to_string(normalize_path_windows(o)) + ";");
                if (c->always)
                    ctx.addText(to_string(normalize_path_windows(int_dir / "rules" / "intentionally_missing.file")) + ";");
                ctx.endBlock(true);

                ctx.beginBlockWithConfiguration("Command", s);
                ctx.addText("call \"" + to_string(normalize_path_windows(cmd)) + "\"");
                ctx.endBlock(true);

                ctx.beginBlockWithConfiguration("BuildInParallel", s);
                ctx.addText("true");
                ctx.endBlock(true);

                ctx.beginBlockWithConfiguration("Message", s);
                ctx.addText(c->getName());
                ctx.endBlock();

                if (c->always && g.vs_version >= sw::PackageVersion::Version(16))
                {
                    ctx.beginBlockWithConfiguration("VerifyInputsAndOutputsExist", s);
                    ctx.addText("false");
                    ctx.endBlock(true);
                }

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
        }

        for (auto &c : d.custom_rules_manual)
        {
            path rule = rules_dir / c.name;
            rule += ".rule";
            if (!fs::exists(rule)) // prevent rebuilds
                write_file(rule, "");
            ((Project &)*this).files.insert({ rule, ". SW Rules" });

            ctx.beginFileBlock(rule);

            ctx.beginBlockWithConfiguration("Outputs", s);
            for (auto &o : c.outputs)
                ctx.addText(to_string(normalize_path_windows(o)) + ";");
            ctx.endBlock(true);

            ctx.beginBlockWithConfiguration("AdditionalInputs", s);
            for (auto &o : c.inputs)
                ctx.addText(to_string(normalize_path_windows(o)) + ";");
            ctx.endBlock(true);

            ctx.beginBlockWithConfiguration("Command", s);
            ctx.addText(c.command);
            ctx.endBlock(true);

            ctx.beginBlockWithConfiguration("Message", s);
            if (!c.message.empty())
                ctx.addText(c.message);
            ctx.endBlock();

            if (g.vs_version >= sw::PackageVersion::Version(16))
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

    if (g.compiler_type == VSGenerator::ClangCl || g.compiler_type == VSGenerator::Clang)
    {
        auto get_prog = [&g](const sw::UnresolvedPackageName &u) -> String
        {
            SW_UNIMPLEMENTED;
            /*auto &target = **g.b->getContext().getPredefinedTargets().find(u)->second.begin();
            auto fn = to_string(normalize_path_windows(target.as<sw::PredefinedProgram &>().getProgram().file));
            return fn;*/
        };

        ctx.beginBlock("PropertyGroup");
        ctx.addBlock("CLToolExe", get_prog((*settings.begin())["native"]["program"]["cpp"].getValue()));
        ctx.addBlock("LIBToolExe", get_prog((*settings.begin())["native"]["program"]["lib"].getValue()));
        ctx.addBlock("LinkToolExe", get_prog((*settings.begin())["native"]["program"]["link"].getValue()));
        ctx.endBlock();

        // taken from llvm/tools/msbuild/LLVM.Cpp.Common.targets
        String clangprops = R"xxx(
    <ItemDefinitionGroup>
      <ClCompile>
        <!-- Map /ZI and /Zi to /Z7.  Clang internally does this, so if we were
             to just pass the option through, clang would work.  The problem is
             that MSBuild would not.  MSBuild detects /ZI and /Zi and then
             assumes (rightly) that there will be a compiler-generated PDB (e.g.
             vc141.pdb).  Since clang-cl will not emit this, MSBuild will always
             think that the compiler-generated PDB needs to be re-generated from
             scratch and trigger a full build.  The way to avoid this is to
             always give MSBuild accurate information about how we plan to
             generate debug info (which is to always using /Z7 semantics).
             -->
        <!-- disable for now
        <DebugInformationFormat Condition="'%(ClCompile.DebugInformationFormat)' == 'ProgramDatabase'">OldStyle</DebugInformationFormat>
        <DebugInformationFormat Condition="'%(ClCompile.DebugInformationFormat)' == 'EditAndContinue'">OldStyle</DebugInformationFormat> -->

        <!-- Unset any options that we either silently ignore or warn about due to compatibility.
             Generally when an option is set to no value, that means "Don't pass an option to the
             compiler at all."
             -->
        <MinimalRebuild/>

        <!-- <WholeProgramOptimization/>
        <EnableFiberSafeOptimizations/>
        <IgnoreStandardIncludePath/>
        <EnableParallelCodeGeneration/>
        <ForceConformanceInForLoopScope/>
        <TreatWChar_tAsBuiltInType/>
        <SDLCheck/>
        <GenerateXMLDocumentationFiles/>
        <BrowseInformation/>
        <EnablePREfast/>
        <StringPooling/>
        <ExpandAttributedSource/>
        <EnforceTypeConversionRules/>
        <ErrorReporting/>
        <DisableLanguageExtensions/>
        <ProgramDataBaseFileName/>
        <DisableSpecificWarnings/>
        <TreatSpecificWarningsAsErrors/>
        <ForcedUsingFiles/>
        <PREfastLog/>
        <PREfastAdditionalOptions/>
        <PREfastAdditionalPlugins/>
        <MultiProcessorCompilation/>
        <UseFullPaths/>
        <RemoveUnreferencedCodeData/> -->

        <!-- We can't just unset BasicRuntimeChecks, as that will pass /RTCu to the compiler.
             We have to explicitly set it to 'Default' to avoid passing anything. -->
        <BasicRuntimeChecks>Default</BasicRuntimeChecks>
      </ClCompile>
    </ItemDefinitionGroup>
)xxx";
        ctx.addLine(clangprops);
    }

    ctx.endProject();
    write_file_if_different(g.sln_root / vs_project_dir / name += vs_project_ext, ctx.getText());
}

void Project::emitFilters(const VSGenerator &g) const
{
    StringSet filters; // dirs

    String sd = to_string(normalize_path(source_dir));

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
        const sw::PackageSettings *s = nullptr; // also mark generated files
        bool bdir_private = false;
        bool bdir_parent = false;
        size_t p = 0;
        auto fd = to_string(normalize_path(f.p));

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
            String bd = to_string(normalize_path(d1.second.binary_dir));
            String bdp = to_string(normalize_path(d1.second.binary_private_dir));
            String bdparent = to_string(normalize_path(d1.second.binary_dir.parent_path()));

            calc(bdparent); // must go first, as shorter path
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

            if (d == &bdparent)
            {
                s = &d1.first;
                bdir_parent = true;
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
                if (bdir_parent)
                {
                    auto v = r;
                    r = "Generated Files";
                    r /= (*s)["os"]["arch"].getValue();
                    r /= get_configuration(*s);
                    r /= "Other" / v;
                }
                else if (!bdir_private)
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

String Project::get_flag_table(const primitives::Command &c, bool throw_on_error)
{
    auto ft = to_string(path(c.getProgram()).stem().u8string());
    if (ft == "ml64")
        ft = "ml";
    else if (ft == "clang-cl")
        ft = "cl";
    if (ft == "clang" || ft == "clang++")
    {
        ft = "clang";
        flag_tables[ft]; // create empty table, so all flags will go to additional options
    }
    if (flag_tables.find(ft) == flag_tables.end())
    {
        if (throw_on_error)
            throw SW_RUNTIME_ERROR("No flag table: " + ft);
        return {};
    }
    return ft;
}

std::map<String, String> Project::printProperties(const sw::builder::Command &c, const Properties &exclude_props) const
{
    auto ft = get_flag_table(c);

    std::map<String, String> args;
    for (int na = 1; na < c.arguments.size(); na++)
    {
        auto &o = c.arguments[na];
        auto arg = o->toString();

        auto add_additional_args = [&args, &ft, &c, &exclude_props, &o](const auto &arg)
        {
            if (exclude_props.exclude_exts.find(path(arg).extension().string()) != exclude_props.exclude_exts.end())
                return;
            if (ft == "ml")
            {
                if (arg == "-c")
                    return;
            }
            if (ft == "cl" || ft == "clang")
            {
                if (arg == "-c" || arg == "-FS")
                    return;
                auto i = c.inputs.find(normalize_path(arg));
                if (i != c.inputs.end())
                    return;
                args["AdditionalOptions"] += o->quote();
                args["AdditionalOptions"] += " ";
                return;
            }
            args["AdditionalDependencies"] += arg + ";";
        };

        if (!arg.empty() && arg[0] != '-' && arg[0] != '/')
        {
            add_additional_args(arg);
            continue;
        }

        auto &tbl = flag_tables[ft].ftable;

        auto print = [&args, &exclude_props, &c, &na, &ft](auto &d, const String &arg)
        {
            if (exclude_props.exclude_flags.find(d.name) != exclude_props.exclude_flags.end())
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
                    args[d.name] += a + ";";
                    return;
                }
                else
                {
                    args[d.name] = a;
                }
            }
            else
            {
                args[d.name] = d.value;
            }
        };

        if (arg.empty())
        {
            LOG_WARN(logger, "Empty arg for command: " + c.print());
            continue;
        }

        // clang
        if (arg == "-fcolor-diagnostics" || arg == "-fansi-escape-codes")
            continue;

        // clang cl
        if (arg == "-Xclang" && na + 1 < c.arguments.size() &&
            (c.arguments[na+1]->toString() == "-fcolor-diagnostics" ||
                c.arguments[na+1]->toString() == "-fansi-escape-codes"))
        {
            na++;
            continue;
        }

        auto find_arg = [&tbl, &print](const String &arg)
        {
            // TODO: we must find the longest match
            bool found = false;
            for (auto &[_, d] : tbl)
            {
                if (d.argument.empty())
                    continue;
                if (arg.find(d.argument, 1) != 1)
                    continue;

                // if flag is matched, but it does not expect user value, we skip it
                // distinct -u vs -utf8
                //                                                                        '/'
                if (!bitmask_includes(d.flags, FlagTableFlags::UserValue) && arg.size() > (1 + d.argument.size()))
                    continue;

                print(d, arg);
                found = true;
                break;
            }
            return found;
        };

        // add system dir both to vs include dirs and additional options
        if (arg.find("-imsvc") == 0)
        {
            auto argi = "-I" + arg.substr(6);
            find_arg(argi);
        }

        // fast lookup first
        auto i = tbl.find(arg.substr(1));
        if (i != tbl.end())
        {
            print(i->second, arg);
            continue;
        }

        auto found = find_arg(arg);
        if (!found)
        {
            //LOG_WARN(logger, "arg not found: " + arg);

            add_additional_args(arg);
            continue;
        }
    }
    return args;
}

void PackagePathTree::add(const sw::PackageName &p)
{
    add(p.getPath(), p);
}

void PackagePathTree::add(const sw::PackagePath &p, const sw::PackageName &project)
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
