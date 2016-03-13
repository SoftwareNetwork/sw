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

#include <iostream>
#include <string>

#include <cppan.h>
#include "options.h"

int main(int argc, char *argv[])
try
{
    ProgramOptions options;
    bool r = options.parseArgs(argc, argv);
    if (!r || options().count("help"))
    {
        std::cout << options.printHelp() << "\n";
        return !r;
    }
    if (options["version"].as<bool>())
    {
        std::cout << get_program_version_string("cppan");
        return 0;
    }
    if (options().count("dir"))
        fs::current_path(options["dir"].as<std::string>());

    auto c = Config::load_user_config();
    c.load_current_config();

    if (options()["prepare-archive"].as<bool>())
    {
        Projects &projects = c.getProjects();
        for (auto &project : projects)
        {
            project.findSources(".");
            String archive_name = make_archive_name(project.package.toString());
            if (!project.writeArchive(archive_name))
                throw std::runtime_error("Archive write failed");
        }
    }
    else
    {
        c.download_dependencies();
        c.create_build_files();
    }

    return 0;
}
catch (const std::exception &e)
{
    std::cout << e.what() << "\n";
    return 1;
}
catch (...)
{
    std::cout << "Unhandled unknown exception" << "\n";
    return 1;
}
