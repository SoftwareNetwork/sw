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

void merge(yaml &to, const yaml &from)
{
    if (!from.IsDefined())
        return;

    // if 'to' node is not a map, make it so
    if (!to.IsMap())
        to = yaml();

    for (auto &f : from)
    {
        auto sf = f.first.as<String>();
        auto ff = f.second.Type();

        bool found = false;
        for (auto t : to)
        {
            const auto st = t.first.as<String>();
            if (sf == st)
            {
                const auto ft = t.second.Type();
                if (ff == YAML::NodeType::Scalar && ft == YAML::NodeType::Scalar)
                {
                    yaml nn;
                    nn.push_back(t.second);
                    nn.push_back(f.second);
                    to[st] = nn;
                }
                else if (ff == YAML::NodeType::Scalar && ft == YAML::NodeType::Sequence)
                {
                    t.second.push_back(f.second);
                }
                else if (ff == YAML::NodeType::Sequence && ft == YAML::NodeType::Scalar)
                {
                    yaml nn = YAML::Clone(f);
                    nn.push_back(t.second);
                    to[st] = nn;
                }
                else if (ff == YAML::NodeType::Sequence && ft == YAML::NodeType::Sequence)
                {
                    for (auto &fv : f)
                        t.second.push_back(fv);
                }
                else if (ff == YAML::NodeType::Map && ft == YAML::NodeType::Map)
                    merge(t.second, f.second);
                else // elaborate more on this?
                    throw std::runtime_error("yaml merge: nodes ('" + sf + "') has incompatible types");
                found = true;
            }
        }
        if (!found)
        {
            to[sf] = f.second;
        }
    }
}

void prepare_yaml_config(yaml &root)
{
    // can be all node checks from config, project, settings moved here?

    //
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

                    root[kv.first] = kv.second;
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
                merge(prj, root["source"]);
                merge(prj, root["version"]);
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
    prepare_yaml_config(root);
    return root;
}
