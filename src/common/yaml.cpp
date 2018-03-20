/*
 * Copyright (C) 2016-2017, Egor Pugin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "yaml.h"

#include "checks.h"
#include "project.h"

#include <boost/algorithm/string.hpp>

void prepare_config_for_reading(yaml &root)
{
    // can be all node checks from config, project, settings moved here?

    // no effect
    if (!root.IsMap())
        return;

    auto prjs = root["projects"];
    if (prjs.IsDefined() && !prjs.IsMap())
        throw std::runtime_error("'projects' should be a map");

    // copy common settings to all subprojects
    {
        auto common_settings = root["common_settings"];
        if (common_settings.IsDefined())
        {
            if (prjs.IsDefined())
            {
                for (auto prj : prjs)
                    BuildSystemConfigInsertions::merge(prj.second, common_settings);
                BuildSystemConfigInsertions::remove(common_settings);

                for (auto prj : prjs)
                    merge(prj.second, common_settings);
            }
            else
            {
                BuildSystemConfigInsertions::merge_and_remove(root, common_settings);
                merge(root, common_settings);
                //throw std::runtime_error("'common_settings' is meaningless when no 'projects' exist");
            }
            root.remove("common_settings");
        }
    }

    if (prjs.IsDefined())
    {
        for (auto prj : prjs)
            BuildSystemConfigInsertions::merge(prj.second, root);
        BuildSystemConfigInsertions::remove(root);

        for (auto prj : prjs)
        {
            // source & version
            YamlMergeFlags flags;
            flags.scalar_scalar = YamlMergeFlags::DontTouchScalars;
            if (!prj.second["source"].IsDefined() && root["source"].IsDefined())
            {
                auto s = prj.second["source"];
                merge(s, root["source"], flags);
            }
            if (!prj.second["version"].IsDefined() && root["version"].IsDefined())
                prj.second["version"] = YAML::Clone(root["version"]);
        }
    }
}

yaml load_yaml_config(const path &p)
{
    auto s = read_file(p);
    return load_yaml_config(s);
}

yaml load_yaml_config(const String &s)
{
    auto root = YAML::Load(s);
    prepare_config_for_reading(root);
    return root;
}

void dump_yaml_config(const path &p, const yaml &root)
{
    write_file(p, dump_yaml_config(root));
}

String dump_yaml_config(const yaml &root)
{
    using namespace YAML;

    if (!root.IsMap())
        return Dump(root);

    Emitter e;
    e.SetIndent(4);
    e << BeginMap;

    auto emit = [&e](auto root, const String &k, bool literal = false)
    {
        e << Key << k;
        e << Value;
        if (literal)
        {
            e << Literal;
            e << boost::trim_copy(root[k].template as<String>());
        }
        else
            e << root[k];
        e << Newline << Newline;
    };

    Strings begin
    {
        "local_settings",

        "source",
        "version",

        "common_settings",

        "root_project",
    };

    Strings project
    {
        "name",
        "license",

        "type",
        "library_type",
        "executable_type",

        "root_directory",
        "root_dir",
        "unpack_directory",
        "unpack_dir",
        "output_directory",
        "output_dir",

        "output_name",
        "condition",

        "c_standard",
        "c",
        "c_extensions",

        "cxx_standard",
        "c++",
        "cxx_extensions",

        "empty",
        "custom",

        "static_only",
        "shared_only",
        "header_only",

        "import_from_bazel",
        "bazel_target_name",
        "bazel_target_function",

        "prefer_binaries",
        "export_all_symbols",
        "export_if_static",
        "rc_enabled",
        "build_dependencies_with_same_config",
        "disabled",

        "api_name",

        "files",
        "build",
        "exclude_from_package",
        "exclude_from_build",
        "public_headers",
        "include_hints",

        "include_directories",
        "options",
        "aliases",
        "checks_prefixes",
        "dependencies", // move above options?

        "patch",
    };

    Strings end;
    for (int i = 0; i < Check::Max; i++)
    {
        auto inf = getCheckInformation(i);
        end.push_back(inf.cppan_key);
    }

    Strings literal = BuildSystemConfigInsertions::getStrings();

    std::set<String> keys;
    keys.insert(begin.begin(), begin.end());
    keys.insert("projects");
    keys.insert(project.begin(), project.end());
    keys.insert(end.begin(), end.end());
    keys.insert(literal.begin(), literal.end());

    auto print = [&emit, &literal](auto root, const auto &v)
    {
        for (auto &b : v)
        {
            if (!root[b].IsDefined())
                continue;
            emit(root, b, std::find(literal.begin(), literal.end(), b) != literal.end());
        }
    };

    auto print_rest = [&keys, &print, &emit, &literal, &end](auto root)
    {
        // emit not enumerated keys
        for (auto n : root)
        {
            auto k = n.first.template as<String>();
            if (keys.find(k) == keys.end())
                emit(root, k);
        }

        print(root, literal);
        print(root, end);
    };

    print(root, begin);

    if (root["projects"].IsDefined())
    {
        e << Key << "projects";
        e << Value;
        e << BeginMap;
        for (auto p : root["projects"])
        {
            auto k = p.first.template as<String>();
            e << Key << k;
            e << Value;
            e << BeginMap;
            // can have source
            print(p.second, begin);
            // main data
            print(p.second, project);
            // rest
            print_rest(p.second);
            e << EndMap;
        }
        e << EndMap;
    }
    else
        print(root, project);

    print_rest(root);

    e << EndMap;
    return e.c_str();
}
