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

#pragma once

#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace bazel
{

using Name = std::string;
using Value = std::string;
using Values = std::set<Value>;

struct Parameter
{
    Name name;
    Values values;

    void trimQuotes();
};

using Parameters = std::vector<Parameter>;

struct Function
{
    Name name;
    Parameters parameters;

    void trimQuotes();
};

using Functions = std::vector<Function>;

struct File
{
    Functions functions;
    std::unordered_map<Name, Parameter> parameters;

    void trimQuotes();
    Values getFiles(const Name &name, const std::string &bazel_target_function = std::string());
};

File parse(const std::string &s);

} // namespace bazel
