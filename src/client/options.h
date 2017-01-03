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

#include <boost/program_options.hpp>

#include <filesystem.h>

namespace po = boost::program_options;

class ProgramOptions
{
public:
    ProgramOptions();

    bool parseArgs(int argc, const char * const *argv);
    bool parseArgs(const Strings &args);
    std::string printHelp() const;

    const po::variable_value& operator[](const std::string& name) const { return vm[name]; }
    const po::variables_map &operator()() const { return vm; }

private:
    po::variables_map vm;
    po::options_description visible;
    po::options_description hidden;
};

#define BUILD_PACKAGES "build-packages"
#define CLEAN_PACKAGES "clean-packages"
#define CLEAN_CONFIGS "clean-configs"
#define SERVER_QUERY "server-query"
