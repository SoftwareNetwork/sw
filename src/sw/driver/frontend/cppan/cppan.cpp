// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include "cppan.h"

#include "project.h"
#include "yaml.h"

#include <sw/driver/build.h>
#include <sw/driver/target/native.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "cppan");

namespace sw::driver::cpp::frontend::cppan
{

static void cppan_load_project(NativeCompiledTarget &t, const yaml &root)
{
    if (root["source"].IsDefined())
        t += Source::load(root["source"]);

    YAML_EXTRACT_AUTO2(t.Empty, "empty");
    YAML_EXTRACT_VAR(root, t.HeaderOnly, "header_only", bool);

    YAML_EXTRACT_AUTO2(t.ImportFromBazel, "import_from_bazel");
    YAML_EXTRACT_AUTO2(t.BazelTargetName, "bazel_target_name");
    YAML_EXTRACT_AUTO2(t.BazelTargetFunction, "bazel_target_function");

    YAML_EXTRACT_AUTO2(t.ExportAllSymbols, "export_all_symbols");
    YAML_EXTRACT_AUTO2(t.ExportIfStatic, "export_if_static");

    t.ApiNames = get_sequence_set<String>(root, "api_name");

    auto read_dir = [&root](auto &p, const String &s)
    {
        get_scalar_f(root, s, [&p, &s](const auto &n)
        {
            auto cp = current_thread_path();
            p = n.template as<String>();
            if (!is_under_root(cp / p, cp))
                throw std::runtime_error("'" + s + "' must not point outside the current dir: " + p.string() + ", " + cp.string());
        });
    };

    // root directory
    path rd;
    read_dir(rd, "root_directory");
    if (rd.empty())
        read_dir(rd, "root_dir");
    t.setRootDirectory(rd);

    // sources
    {
        auto read_sources = [&root](auto &a, const String &key, bool required = true)
        {
            a.clear();
            auto files = root[key];
            if (!files.IsDefined())
                return;
            if (files.IsScalar())
            {
                a.insert(files.as<String>());
            }
            else if (files.IsSequence())
            {
                for (const auto &v : files)
                    a.insert(v.as<String>());
            }
            else if (files.IsMap())
            {
                for (const auto &group : files)
                {
                    if (group.second.IsScalar())
                        a.insert(group.second.as<String>());
                    else if (group.second.IsSequence())
                    {
                        for (const auto &v : group.second)
                            a.insert(v.as<String>());
                    }
                    else if (group.second.IsMap())
                    {
                        String root = get_scalar<String>(group.second, "root");
                        auto v = get_sequence<String>(group.second, "files");
                        for (auto &e : v)
                            a.insert(root + "/" + e);
                    }
                }
            }
        };

        StringSet sources;
        read_sources(sources, "files");
        for (auto &s : sources)
            t += FileRegex(t.SourceDir, std::regex(s), true);

        StringSet exclude_from_build;
        read_sources(exclude_from_build, "exclude_from_build");
        for (auto &s : exclude_from_build)
            t -= FileRegex(t.SourceDir, std::regex(s), true);

        StringSet exclude_from_package;
        read_sources(exclude_from_package, "exclude_from_package");
        for (auto &s : exclude_from_package)
            t ^= FileRegex(t.SourceDir, std::regex(s), true);
    }

    // include_directories
    {
        get_variety(root, "include_directories",
            [&t](const auto &d)
        {
            t.Public.IncludeDirectories.insert(d.template as<String>());
        },
            [&t](const auto &dall)
        {
            for (auto d : dall)
                t.Public.IncludeDirectories.insert(d.template as<String>());
        },
            [&t, &root](const auto &)
        {
            get_map_and_iterate(root, "include_directories", [&t](const auto &n)
            {
                auto f = n.first.template as<String>();
                auto s = get_sequence<String>(n.second);
                if (f == "public")
                    t.Public.IncludeDirectories.insert(s.begin(), s.end());
                else if (f == "private")
                    t.Private.IncludeDirectories.insert(s.begin(), s.end());
                else if (f == "interface")
                    t.Interface.IncludeDirectories.insert(s.begin(), s.end());
                else if (f == "protected")
                    t.Protected.IncludeDirectories.insert(s.begin(), s.end());
                else
                    throw std::runtime_error("include key must be only 'public' or 'private' or 'interface'");
            });
        });
    }

    // deps
    {
        auto read_version = [](auto &dependency, const String &v)
        {
            // some code was removed here
            // check out original version (v1) if you encounter some errors

            //auto nppath = dependency.getPath() / v;
            //dependency.getPath() = nppath;

            dependency.range = v;
        };

        auto relative_name_to_absolute = [](const String &in)
        {
            // TODO
            PackagePath p(in);
            return p;
            //throw SW_RUNTIME_ERROR("not implemented");
            //return in;
        };

        auto read_single_dep = [&t, &read_version, &relative_name_to_absolute](const auto &d, UnresolvedPackage dependency = {})
        {
            bool local_ok = false;
            if (d.IsScalar())
            {
                auto p = extractFromString(d.template as<String>());
                dependency.ppath = relative_name_to_absolute(p.getPath().toString());
                dependency.range = p.range;
            }
            else if (d.IsMap())
            {
                // read only field related to ppath - name, local
                if (d["name"].IsDefined())
                    dependency.ppath = relative_name_to_absolute(d["name"].template as<String>());
                if (d["package"].IsDefined())
                    dependency.ppath = relative_name_to_absolute(d["package"].template as<String>());
                if (dependency.ppath.empty() && d.size() == 1)
                {
                    dependency.ppath = relative_name_to_absolute(d.begin()->first.template as<String>());
                    //if (dependency.ppath.is_loc())
                        //dependency.flags.set(pfLocalProject);
                    read_version(dependency, d.begin()->second.template as<String>());
                }
                if (d["local"].IsDefined()/* && allow_local_dependencies*/)
                {
                    auto p = d["local"].template as<String>();
                    UnresolvedPackage pkg;
                    pkg.ppath = p;
                    //if (rd.known_local_packages.find(pkg) != rd.known_local_packages.end())
                        //local_ok = true;
                    if (local_ok)
                        dependency.ppath = p;
                }
            }

            if (dependency.ppath.is_loc())
            {
                //dependency.flags.set(pfLocalProject);

                // version will be read for local project
                // even 2nd arg is not valid
                String v;
                if (d.IsMap() && d["version"].IsDefined())
                    v = d["version"].template as<String>();
                read_version(dependency, v);
            }

            if (d.IsMap())
            {
                // read other map fields
                if (d["version"].IsDefined())
                {
                    read_version(dependency, d["version"].template as<String>());
                    if (local_ok)
                        dependency.range = "*";
                }
                //if (d["ref"].IsDefined())
                    //dependency.reference = d["ref"].template as<String>();
                //if (d["reference"].IsDefined())
                    //dependency.reference = d["reference"].template as<String>();
                //if (d["include_directories_only"].IsDefined())
                    //dependency.flags.set(pfIncludeDirectoriesOnly, d["include_directories_only"].template as<bool>());

                // conditions
                //dependency.conditions = get_sequence_set<String>(d, "condition");
                //auto conds = get_sequence_set<String>(d, "conditions");
                //dependency.conditions.insert(conds.begin(), conds.end());
            }

            //if (dependency.flags[pfLocalProject])
                //dependency.createNames();

            return dependency;
        };

        auto get_deps = [&](const auto &node)
        {
            get_variety(root, node,
                [&t, &read_single_dep](const auto &d)
            {
                auto dep = read_single_dep(d);
                t.Public += dep;
                //throw SW_RUNTIME_ERROR("not implemented");
                //dependencies[dep.ppath.toString()] = dep;
            },
                [&t, &read_single_dep](const auto &dall)
            {
                for (auto d : dall)
                {
                    auto dep = read_single_dep(d);
                    t.Public += dep;
                    //throw SW_RUNTIME_ERROR("not implemented");
                    //dependencies[dep.ppath.toString()] = dep;
                }
            },
                [&t, &read_single_dep, &read_version, &relative_name_to_absolute](const auto &dall)
            {
                auto get_dep = [this, &read_version, &read_single_dep, &relative_name_to_absolute](const auto &d)
                {
                    UnresolvedPackage dependency;

                    dependency.ppath = relative_name_to_absolute(d.first.template as<String>());
                    //if (dependency.ppath.is_loc())
                        //dependency.flags.set(pfLocalProject);

                    if (d.second.IsScalar())
                        read_version(dependency, d.second.template as<String>());
                    else if (d.second.IsMap())
                        return read_single_dep(d.second, dependency);
                    else
                        throw std::runtime_error("Dependency should be a scalar or a map");

                    //if (dependency.flags[pfLocalProject])
                        //dependency.createNames();

                    return dependency;
                };

                auto extract_deps = [&get_dep, &read_single_dep](const auto &dall, const auto &str)
                {
                    UnresolvedPackages deps;
                    auto priv = dall[str];
                    if (!priv.IsDefined())
                        return deps;
                    if (priv.IsMap())
                    {
                        get_map_and_iterate(dall, str,
                            [&get_dep, &deps](const auto &d)
                        {
                            auto dep = get_dep(d);
                            deps.insert(dep);
                            //throw SW_RUNTIME_ERROR("not implemented");
                            //deps[dep.ppath.toString()] = dep;
                        });
                    }
                    else if (priv.IsSequence())
                    {
                        for (auto d : priv)
                        {
                            auto dep = read_single_dep(d);
                            deps.insert(dep);
                            //throw SW_RUNTIME_ERROR("not implemented");
                            //deps[dep.ppath.toString()] = dep;
                        }
                    }
                    return deps;
                };

                auto extract_deps_from_node = [&t, &extract_deps, &get_dep](const auto &node)
                {
                    auto deps_private = extract_deps(node, "private");
                    auto deps = extract_deps(node, "public");

                    t += deps_private;
                    for (auto &d : deps_private)
                    {
                        //operator+=(d);
                        //throw SW_RUNTIME_ERROR("not implemented");
                        //d.second.flags.set(pfPrivateDependency);
                        //deps.insert(d);
                    }

                    t.Public += deps;
                    for (auto &d : deps)
                    {
                        //Public += d;
                        //throw SW_RUNTIME_ERROR("not implemented");
                        //d.second.flags.set(pfPrivateDependency);
                        //deps.insert(d);
                    }

                    if (deps.empty() && deps_private.empty())
                    {
                        for (auto d : node)
                        {
                            auto dep = get_dep(d);
                            t.Public += dep;
                            //throw SW_RUNTIME_ERROR("not implemented");
                            //deps[dep.ppath.toString()] = dep;
                        }
                    }

                    return deps;
                };

                auto ed = extract_deps_from_node(dall);
                //throw SW_RUNTIME_ERROR("not implemented");
                //dependencies.insert(ed.begin(), ed.end());

                // conditional deps
                /*for (auto n : dall)
                {
                    auto spec = n.first.as<String>();
                    if (spec == "private" || spec == "public")
                        continue;
                    if (n.second.IsSequence())
                    {
                        for (auto d : n.second)
                        {
                            auto dep = read_single_dep(d);
                            dep.condition = spec;
                            dependencies[dep.ppath.toString()] = dep;
                        }
                    }
                    else if (n.second.IsMap())
                    {
                        ed = extract_deps_from_node(n.second, spec);
                        dependencies.insert(ed.begin(), ed.end());
                    }
                }

                if (deps.empty() && deps_private.empty())
                {
                    for (auto d : node)
                    {
                        auto dep = get_dep(d);
                        deps[dep.ppath.toString()] = dep;
                    }
                }*/
            });
        };

        get_deps("dependencies");
        get_deps("deps");
    }

    // standards
    {
        int c_standard = 89;
        bool c_extensions = false;
        YAML_EXTRACT_AUTO(c_standard);
        if (c_standard == 0)
        {
            YAML_EXTRACT_VAR(root, c_standard, "c", int);
        }
        YAML_EXTRACT_AUTO(c_extensions);

        int cxx_standard = 14;
        bool cxx_extensions = false;
        String cxx;
        YAML_EXTRACT_VAR(root, cxx, "cxx_standard", String);
        if (cxx.empty())
            YAML_EXTRACT_VAR(root, cxx, "c++", String);
        if (cxx.empty())
            YAML_EXTRACT_VAR(root, cxx, "cpp", String);
        YAML_EXTRACT_AUTO(cxx_extensions);

        if (!cxx.empty())
        {
            try
            {
                cxx_standard = std::stoi(cxx);
            }
            catch (const std::exception&)
            {
                if (cxx == "1z")
                    cxx_standard = 17;
                else if (cxx == "2x")
                    cxx_standard = 20;
            }
        }

        switch (cxx_standard)
        {
        case 98:
            t.CPPVersion = CPPLanguageStandard::CPP98;
            break;
        case 11:
            t.CPPVersion = CPPLanguageStandard::CPP11;
            break;
        case 14:
            t.CPPVersion = CPPLanguageStandard::CPP14;
            break;
        case 17:
            t.CPPVersion = CPPLanguageStandard::CPP17;
            break;
        case 20:
            t.CPPVersion = CPPLanguageStandard::CPP20;
            break;
        }
    }

    /*YAML_EXTRACT_AUTO(output_name);
    YAML_EXTRACT_AUTO(condition);
    YAML_EXTRACT_AUTO(include_script);
    license = get_scalar<String>(root, "license");

    read_dir(unpack_directory, "unpack_directory");
    if (unpack_directory.empty())
        read_dir(unpack_directory, "unpack_dir");

    YAML_EXTRACT_AUTO(output_directory);
    if (output_directory.empty())
        YAML_EXTRACT_VAR(root, output_directory, "output_dir", String);

    bs_insertions.load(root);*/
    auto options = ::sw::cppan::loadOptionsMap(root);
    for (auto &[k, v] : options["shared"].definitions)
        t.add(Definition(v));
    for (auto &[k, v] : options["any"].system_definitions["win32"])
        t.add(Definition(v));
    for (auto &[k, v] : options["any"].system_link_libraries["win32"])
        t.add(SystemLinkLibrary(v));

    /*read_sources(public_headers, "public_headers");
    include_hints = get_sequence_set<String>(root, "include_hints");

    aliases = get_sequence_set<String>(root, "aliases");

    checks.load(root);
    checks_prefixes = get_sequence_set<String>(root, "checks_prefixes");
    if (checks_prefixes.empty())
        checks_prefixes = get_sequence_set<String>(root, "checks_prefix");

    const auto &patch_node = root["patch"];
    if (patch_node.IsDefined())
        patch.load(patch_node);*/
}

static void cppan_load_project(ExecutableTarget &t, const yaml &root)
{
    String et;
    bool et2 = false;
    YAML_EXTRACT_VAR(root, et, "executable_type", String);
    YAML_EXTRACT_VAR(root, et2, "win32", bool);
    if (et == "win32" || et2)
    {
        if (auto L = t.getSelectedTool()->as<VisualStudioLinker *>(); L)
            L->Subsystem = vs::Subsystem::Windows;
    }

    cppan_load_project((NativeCompiledTarget&)t, root);
}

static std::vector<NativeCompiledTarget *> cppan_load1(Build &b, const yaml &root, const String &root_name)
{
    if (root.IsNull() || !root.IsMap())
        throw SW_RUNTIME_ERROR("Spec file should be a map");

    // load subdirs
    /*auto sd = root["add_directories"];
    decltype(projects) subdir_projects;
    if (sd.IsDefined())
    {
        decltype(projects) all_projects;
        for (const auto &v : sd)
        {
            auto load_specific = [this, &subdir_projects](const path &d, const String &name)
            {
                if (!fs::exists(d))
                    return;

                SwapAndRestore r(subdir, name);
                reload(d);

                for (auto &[s, p] : projects)
                    rd.known_local_packages.insert(p.pkg);

                subdir_projects.insert(projects.begin(), projects.end());
                projects.clear();
            };

            if (v.IsScalar())
            {
                path d = v.as<String>();
                load_specific(d, d.filename().string());
            }
            else if (v.IsMap())
            {
                path d;
                if (v["dir"].IsDefined())
                    d = v["dir"].as<String>();
                else if (v["directory"].IsDefined())
                    d = v["directory"].as<String>();
                if (!fs::exists(d))
                    continue;
                String name = d.filename().string();
                if (v["name"].IsDefined())
                    name = v["name"].as<String>();
                if (v["load_all_packages"].IsDefined() && v["load_all_packages"].as<bool>())
                {
                    SwapAndRestore r(subdir, name);
                    reload(d);

                    for (auto &[s, p] : projects)
                        rd.known_local_packages.insert(p.pkg);

                    all_projects.insert(projects.begin(), projects.end());
                    projects.clear();
                }
                else
                    load_specific(d, name);
                // add skip option? - skip specific
            }
        }
        projects = all_projects;
    }*/

    //load_settings(root);

    PackagePath root_project;
    YAML_EXTRACT(root_project, String);

    Version version;
    YAML_EXTRACT(version, String);

    auto prjs = root["projects"];
    if (prjs.IsDefined() && !prjs.IsMap())
        throw std::runtime_error("'projects' should be a map");

    auto add_project = [&b, &root_project](auto &root, String name, Version version, bool name_unnamed = false) -> NativeCompiledTarget&
    {
        /*Project project(root_project);
        project.defaults_allowed = defaults_allowed;
        project.allow_relative_project_names = allow_relative_project_names;
        project.allow_local_dependencies = allow_local_dependencies;
        project.is_local = is_local;
        project.subdir = subdir;*/

        if (name.empty())
        {
            YAML_EXTRACT_AUTO(name);
            if (name.empty())
            {
                LOG_WARN(logger, "Unnamed target, set 'name: ...' directive");
                if (name_unnamed)
                    name = "unnamed";
                else
                    throw SW_RUNTIME_ERROR("Unnamed target");
            }
        }

        YAML_EXTRACT(version, String);

        String pt;
        YAML_EXTRACT_VAR(root, pt, "type", String);
        if (pt == "l" || pt == "lib" || pt == "library")
            ;
        else if (pt.empty() || pt == "e" || pt == "exe" || pt == "executable")
        {
            auto &t = b.addExecutable(name, version);
            cppan_load_project(t, root);
            return t;
        }
        else
            throw SW_RUNTIME_ERROR("Unknown project type");

        bool shared_only = false;
        bool static_only = false;
        YAML_EXTRACT_AUTO(shared_only);
        YAML_EXTRACT_AUTO(static_only);

        if (shared_only && static_only)
            throw std::runtime_error("Project cannot be static and shared simultaneously");

        String lt = "shared";
        YAML_EXTRACT_VAR(root, lt, "library_type", String);
        if (lt == "static" || static_only)
        {
            auto &t = b.addStaticLibrary(name, version);
            cppan_load_project(t, root);
            return t;
        }
        else if (lt == "shared" || lt == "dll" || shared_only)
        {
            auto &t = b.addSharedLibrary(name, version);
            cppan_load_project(t, root);
            return t;
        }
        else if (lt.empty())
            throw SW_RUNTIME_ERROR(name + ": empty library type");
        else
            throw SW_RUNTIME_ERROR(name + ": unknown library type: " + lt);

        //if (project.name.empty())
            //project.name = name;
        //project.setRelativePath(name);
        // after setRelativePath()
        //if (!subdir.empty())
            //project.name = subdir + "." + project.name;
        //projects.emplace(project.pkg.ppath.toString(), project);
    };

    std::vector<NativeCompiledTarget *> targets;

    if (prjs.IsDefined())
    {
        for (auto prj : prjs)
        {
            auto &t = add_project(prj.second, prj.first.template as<String>(), version);
            targets.push_back(&t);
        }
    }
    else if (root_name.empty())
    {
        auto &t = add_project(root, {}, version, true);
        targets.push_back(&t);
    }
    else
    {
        auto &t = add_project(root, root_name, version);
        targets.push_back(&t);
    }

    // remove unreferences projects
    /*if (sd.IsDefined())
    {
        while (1)
        {
            bool added = false;
            decltype(projects) included_projects;
            for (auto &[pkg, p] : projects)
            {
                for (auto &[n, p2] : p.dependencies)
                {
                    auto i = subdir_projects.find(n);
                    if (i != subdir_projects.end() && projects.find(n) == projects.end())
                    {
                        included_projects.insert(*i);
                        added = true;
                    }
                }
            }
            projects.insert(included_projects.begin(), included_projects.end());
            if (!added)
                break;
        }
    }*/

    return targets;
}

std::vector<NativeCompiledTarget *> cppan_load(Build &b, yaml &root, const String &root_name)
{
    auto root1 = ::sw::cppan::load_yaml_config(root);
    return cppan_load1(b, root1, root_name);
}

}
