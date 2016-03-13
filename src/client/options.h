/*
 * C++ Archive Network Client
 * Copyright (C) 2016 Egor Pugin
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <boost/program_options.hpp>

#include <filesystem.h>

namespace po = boost::program_options;

class ProgramOptions
{
public:
    ProgramOptions();

    bool parseArgs(int argc, char *argv[]);
    std::string printHelp() const;

    const po::variable_value& operator[](const std::string& name) const { return vm[name]; }
    const po::variables_map &operator()() const { return vm; }

private:
    po::variables_map vm;
    po::options_description desc;
};
