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

#include <iostream>
#include <string>

#include <cppan.h>
#include "options.h"

int main(int argc, char *argv[])
try
{
    // default run
    if (argc == 1)
    {
        auto c = Config::load_user_config();
        c.load_current_config();
        c.download_dependencies();
        c.create_build_files();
        return 0;
    }

    // command selector
    if (argv[1][0] != '-')
    {
        // config
        // self-upgrade
        return 0;
    }

    // default command run

    ProgramOptions options;
    bool r = options.parseArgs(argc, argv);

    if (!r || options().count("help"))
    {
        std::cout << options.printHelp() << "\n";
        return !r;
    }
    if (options["version"].as<bool>())
    {
        std::cout << get_program_version_string("cppan") << "\n";
        return 0;
    }
    if (options().count("dir"))
        fs::current_path(options["dir"].as<std::string>());
    httpSettings.verbose = options["curl-verbose"].as<bool>();

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
