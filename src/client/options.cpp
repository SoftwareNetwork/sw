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

#include "options.h"

#include <iostream>
#include <sstream>

ProgramOptions::ProgramOptions()
    :
    visible("Allowed options"),
    hidden("Hidden options")
{
    visible.add_options()
        ("help,h", "produce this message")
        ("dir,d", po::value<std::string>(), "working directory")
        ("version,v", po::bool_switch(), "version")
        ("prepare-archive", po::bool_switch(), "prepare archive locally")
        ("curl-verbose", po::bool_switch(), "set curl to verbose mode")
        ("self-upgrade", po::bool_switch(), "upgrade CPPAN client to the latest version")
        ("ignore-ssl-checks,k", po::bool_switch(), "ignore ssl checks and errors")

        (SERVER_QUERY ",s", po::bool_switch(), "force query server")

        ("config", po::value<std::string>()->default_value(""), "config name for building")
        ("build", po::value<std::string>(), "file or dir: an inline building")
        ("build-only", po::value<std::string>(), "file or dir: an inline building without touching any configs")
        //("rebuild", po::value<std::string>(), "file or dir: an inline building while removing previous version")
        ("generate", po::value<std::string>(), "file or dir: an inline project generation")
        ("dry-run", po::value<std::string>(), "dir: locally simulate project download and build")

        ("settings", po::value<std::string>()->default_value(""), "file to take settings from")
        ("build-package", po::value<std::string>(), "build existing cppan package")

        ("clear-cache", po::bool_switch(), "clear CMakeCache.txt files")
        ("clear-vars-cache", po::bool_switch(), "clear checked symbols, types, includes etc.")
        (CLEAN_PACKAGES, po::value<std::string>(), "completely clean packages files for matched regex")
        ;

    // i - internal options
    hidden.add_options()
        ;
}

bool ProgramOptions::parseArgs(int argc, char *argv[])
{
    try
    {
        po::options_description all("Allowed options");
        all.add(visible).add(hidden);

        po::store(po::parse_command_line(argc, argv, all), vm);
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
    oss << visible;
    return oss.str();
}
