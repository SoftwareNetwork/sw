#pragma once

#include <set>
#include <string>
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
};

using Parameters = std::vector<Parameter>;

struct Function
{
	Name name;
	Parameters parameters;
};

using Functions = std::vector<Function>;

struct File
{
	Functions functions;
};

} // namespace bazel
