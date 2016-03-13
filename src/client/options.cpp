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

#include "options.h"

#include <iostream>
#include <sstream>

ProgramOptions::ProgramOptions()
    : desc("Allowed options")
{
    desc.add_options()
        ("help,h", "produce this message")
        ("dir,d", po::value<std::string>(), "working directory")
        ("version,v", po::bool_switch(), "version")
        ("prepare-archive", po::bool_switch(), "prepare archive locally")
        //("build-app", po::value<std::vector<std::string>>(), "download and build requested executable")
        //("gen-dummy-config", po::bool_switch(), "download and build requested executable")
        ;
}

bool ProgramOptions::parseArgs(int argc, char *argv[])
{
    try
    {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
    }
    catch (const po::error &e)
    {
        std::cerr << e.what() << "\n\n";
        return false;
    }
    return true;
}

std::string ProgramOptions::printHelp() const
{
    std::ostringstream oss;
    oss << desc;
    return oss.str();
}
