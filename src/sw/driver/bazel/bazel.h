/*
 * SW - Build System and Package Manager
 * Copyright (C) 2016-2020 Egor Pugin
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
    Values getFiles(const Name &name, const std::string &bazel_target_function = std::string()) const;
};

File parse(const std::string &s);

} // namespace bazel
