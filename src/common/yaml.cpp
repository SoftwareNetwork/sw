/*
 * Copyright (c) 2016, Egor Pugin
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     1. Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *     3. Neither the name of the copyright holder nor the names of
 *        its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "yaml.h"

#include "checks.h"

#include <boost/algorithm/string.hpp>

// no links allowed
// to do this we call YAML::Clone()
void merge(yaml dst, const yaml &src, const YamlMergeFlags &flags)
{
    if (!src.IsDefined())
        return;

    // if 'dst' node is not a map, make it so
    if (!dst.IsMap())
        dst = yaml();

    for (auto &f : src)
    {
        auto sf = f.first.as<String>();
        auto ff = f.second.Type();

        bool found = false;
        for (auto t : dst)
        {
            const auto st = t.first.as<String>();
            if (sf == st)
            {
                const auto ft = t.second.Type();
                if (ff == YAML::NodeType::Scalar && ft == YAML::NodeType::Scalar)
                {
                    switch (flags.scalar_scalar)
                    {
                    case YamlMergeFlags::ScalarsToSet:
                    {
                        yaml nn;
                        nn.push_back(t.second);
                        nn.push_back(f.second);
                        dst[st] = nn;
                        break;
                    }
                    case YamlMergeFlags::OverwriteScalars:
                        dst[st] = YAML::Clone(src[sf]);
                        break;
                    case YamlMergeFlags::DontTouchScalars:
                        break;
                    }
                }
                else if (ff == YAML::NodeType::Scalar && ft == YAML::NodeType::Sequence)
                {
                    t.second.push_back(f.second);
                }
                else if (ff == YAML::NodeType::Sequence && ft == YAML::NodeType::Scalar)
                {
                    yaml nn = YAML::Clone(f);
                    nn.push_back(t.second);
                    dst[st] = nn;
                }
                else if (ff == YAML::NodeType::Sequence && ft == YAML::NodeType::Sequence)
                {
                    for (auto &fv : f)
                        t.second.push_back(YAML::Clone(fv));
                }
                else if (ff == YAML::NodeType::Map && ft == YAML::NodeType::Map)
                    merge(t.second, f.second, flags);
                else // elaborate more on this?
                    throw std::runtime_error("yaml merge: nodes ('" + sf + "') has incompatible types");
                found = true;
            }
        }
        if (!found)
        {
            dst[sf] = YAML::Clone(f.second);
        }
    }
}

void prepare_config_for_reading(yaml root)
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
        const auto &common_settings = root["common_settings"];
        if (common_settings.IsDefined())
        {
            if (prjs.IsDefined())
            {
                for (auto prj : prjs)
                    merge(prj.second, common_settings);
            }
            else
                merge(root, common_settings);
            root.remove("common_settings");
        }
    }

    // merge bs insertions
    {
        if (prjs.IsDefined())
        {
            for (auto prj : prjs)
            {
#define PREPEND_ROOT_BSI(x)                        \
    if (root[x].IsDefined() && prj[x].IsDefined()) \
    root[x] = root[x].as<String>() + "\n\n" + prj[x].as<String>()

                // concat bs insertions, root's first
                PREPEND_ROOT_BSI("pre_sources");
                PREPEND_ROOT_BSI("post_sources");
                PREPEND_ROOT_BSI("post_target");
                PREPEND_ROOT_BSI("post_alias");
#undef PREPEND_ROOT_BSI

                // if there's no such node in child, add it
                for (auto kv : prj)
                {
                    auto f = kv.first.as<String>();

#define CHECK_BSI(x) if (f == x && root[x].IsDefined()) continue
                    // do not copy if already concat'ed
                    CHECK_BSI("pre_sources");
                    CHECK_BSI("post_sources");
                    CHECK_BSI("post_target");
                    CHECK_BSI("post_alias");
#undef CHECK_BSI

                    root[kv.first] = YAML::Clone(kv.second);
                }
            }
        }
    }

    // source & version
    {
        if (prjs.IsDefined())
        {
            for (auto prj : prjs)
            {
                YamlMergeFlags flags;
                flags.scalar_scalar = YamlMergeFlags::DontTouchScalars;
                if (!prj.second["source"].IsDefined() && root["source"].IsDefined())
                    merge(prj.second["source"], root["source"], flags);
                if (!prj.second["version"].IsDefined() && root["version"].IsDefined())
                    prj.second["version"] = YAML::Clone(root["version"]);
            }
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

        "c_standard",
        "c",
        "cxx_standard",
        "c++",

        "empty",
        "custom",

        "static_only",
        "shared_only",
        "header_only",

        "import_from_bazel",
        "prefer_binaries",
        "export_all_symbols",
        "build_dependencies_with_same_config",

        "api_name",

        "files",
        "build",
        "exclude_from_package",
        "exclude_from_build",

        "include_directories",
        "options",
        "aliases",
        "dependencies",

        "patch",
    };

    Strings end;
    for (int i = 0; i < Check::Max; i++)
    {
        auto inf = getCheckInformation(i);
        end.push_back(inf.cppan_key);
    }

    Strings literal
    {
        "pre_sources",
        "post_sources",
        "post_target",
        "post_alias",
    };

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
