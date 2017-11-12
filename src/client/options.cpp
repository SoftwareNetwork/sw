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
        ("version,V", po::bool_switch(), "version")
        ("prepare-archive", po::bool_switch(), "prepare archive locally")
        ("prepare-archive-remote", po::bool_switch(), "prepare archive from remote source")
        ("curl-verbose", po::bool_switch(), "set curl to verbose mode")
        ("self-upgrade", po::bool_switch(), "upgrade CPPAN client to the latest version")
        ("ignore-ssl-checks,k", po::bool_switch(), "ignore ssl checks and errors")

        (SERVER_QUERY ",s", po::bool_switch(), "force query server")

        ("verify", po::value<std::string>(), "verify package")

        ("config", po::value<std::string>()->default_value(""), "config name for building")
        ("generate", po::value<std::string>(), "file or dir: generate project files for inline building")
        ("build", po::value<std::string>(), "file or dir: inline building")
        ("build-only", po::value<std::string>(), "file or dir: inline building without touching any configs")
        (BUILD_PACKAGES, po::value<Strings>()->multitoken(), "build existing cppan package")

        ("settings", po::value<std::string>()->default_value(""), "file to take settings from")

        ("verbose,v", po::bool_switch(), "verbose output")
        ("trace", po::bool_switch(), "trace output")

        ("clear-cache", po::bool_switch(), "clear CMakeCache.txt files")
        ("clear-vars-cache", po::bool_switch(), "clear checked symbols, types, includes etc.")
        (CLEAN_PACKAGES, po::value<Strings>()->multitoken(), "completely clean package files for matched regex")
        (CLEAN_CONFIGS, po::value<Strings>()->multitoken(), "clean config dirs and files")

        ("beautify", po::value<String>(), "beautify yaml script")
        ("beautify-strict", po::value<String>(), "convert to strict cppan config")
        ("print-cpp", po::value<String>(), "print config's values in cpp style")
        ("print-cpp2", po::value<String>(), "print config's values in cpp style 2")
        ;

    // i - internal options
    hidden.add_options()
        ;
}

bool ProgramOptions::parseArgs(int argc, const char * const * argv)
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

bool ProgramOptions::parseArgs(const Strings &args)
{
    std::vector<const char *> argv;
    for (const auto &a : args)
        argv.push_back(&a[0]);
    return parseArgs((int)argv.size(), &argv[0]);
}

std::string ProgramOptions::printHelp() const
{
    std::ostringstream oss;
    oss << visible;
    return oss.str();
}
