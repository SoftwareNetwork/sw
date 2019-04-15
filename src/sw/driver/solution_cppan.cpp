// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "solution.h"

#include "frontend/cppan/yaml.h"
#include "functions.h"
#include "generator/generator.h"
#include "inserts.h"
#include "module.h"
#include "run.h"
#include "solution_build.h"
#include "sw_abi_version.h"
#include "target/native.h"

#include <sw/builder/execution_plan.h>
#include <sw/builder/file_storage.h>
#include <sw/builder/program.h>
#include <sw/manager/database.h>
#include <sw/manager/resolver.h>
#include <sw/manager/settings.h>
#include <sw/manager/storage.h>

#include <boost/algorithm/string.hpp>
#include <boost/thread/lock_types.hpp>
#include <primitives/date_time.h>
#include <primitives/executor.h>
#include <primitives/pack.h>
#include <primitives/symbol.h>
#include <primitives/templates.h>
#include <primitives/win32helpers.h>
#include <primitives/sw/settings.h>

#include <boost/dll.hpp>
#include <nlohmann/json.hpp>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "solution_cppan");

namespace sw
{

bool Build::cppan_check_config_root(const yaml &root)
{
    if (root.IsNull() || !root.IsMap())
    {
        LOG_DEBUG(logger, "Spec file should be a map");
        return false;
    }
    return true;
}

void Build::cppan_load()
{
    if (solutions.empty())
        addSolution();

    for (auto &s : solutions)
    {
        current_solution = &s;
        cppan_load(config.value());
    }
}

void Build::cppan_load(const path &fn)
{
    auto root = cppan::load_yaml_config(fn);
    cppan_load(root);
}

void Build::cppan_load(const yaml &root, const String &root_name)
{
    if (!cppan_check_config_root(root))
        return;

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

    auto prjs = root["projects"];
    if (prjs.IsDefined() && !prjs.IsMap())
        throw std::runtime_error("'projects' should be a map");

    auto add_project = [this, &root_project](auto &root, String name)
    {
        /*Project project(root_project);
        project.defaults_allowed = defaults_allowed;
        project.allow_relative_project_names = allow_relative_project_names;
        project.allow_local_dependencies = allow_local_dependencies;
        project.is_local = is_local;
        project.subdir = subdir;*/

        if (name.empty())
            YAML_EXTRACT_AUTO(name);

        String pt;
        YAML_EXTRACT_VAR(root, pt, "type", String);
        if (pt == "l" || pt == "lib" || pt == "library")
            ;
        else if (pt.empty() || pt == "e" || pt == "exe" || pt == "executable")
            return current_solution->addExecutable(name).cppan_load_project(root);
        else
            throw SW_RUNTIME_ERROR("Unknown project type");

        bool shared_only = false;
        bool static_only = false;
        YAML_EXTRACT_AUTO(shared_only);
        YAML_EXTRACT_AUTO(static_only);

        if (shared_only && static_only)
            throw std::runtime_error("Project cannot be static and shared simultaneously");

        String lt;
        YAML_EXTRACT_VAR(root, lt, "library_type", String);
        if (lt == "static" || static_only)
            return current_solution->addStaticLibrary(name).cppan_load_project(root);
        else if (lt == "shared" || lt == "dll" || shared_only)
            return current_solution->addSharedLibrary(name).cppan_load_project(root);
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

    if (prjs.IsDefined())
    {
        for (auto prj : prjs)
            add_project(prj.second, prj.first.template as<String>());
    }
    else
    {
        add_project(root, root_name);
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
}

}
