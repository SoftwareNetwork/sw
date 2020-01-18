// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "frontend/cppan/yaml.h"
#include "build.h"
#include "target/native.h"

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "solution_cppan");

namespace sw
{

std::vector<NativeCompiledTarget *> Build::cppan_load(yaml &root, const String &root_name)
{
    auto root1 = cppan::load_yaml_config(root);
    return cppan_load1(root1, root_name);
}

std::vector<NativeCompiledTarget *> Build::cppan_load1(const yaml &root, const String &root_name)
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

    auto add_project = [this, &root_project](auto &root, String name, Version version, bool name_unnamed = false) -> NativeCompiledTarget&
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
            return addExecutable(name, version);
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
            return addStaticLibrary(name, version);
        else if (lt == "shared" || lt == "dll" || shared_only)
            return addSharedLibrary(name, version);
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
            t.cppan_load_project(prj.second);
            targets.push_back(&t);
        }
    }
    else if (root_name.empty())
    {
        auto &t = add_project(root, {}, version, true);
        t.cppan_load_project(root);
        targets.push_back(&t);
    }
    else
    {
        auto &t = add_project(root, root_name, version);
        t.cppan_load_project(root);
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

}
