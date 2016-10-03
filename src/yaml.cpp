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

void merge(const yaml &from, yaml &to)
{
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
                    merge(f.second, t.second);
                else
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
