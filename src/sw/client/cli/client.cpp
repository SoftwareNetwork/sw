/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2020 Egor Pugin
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

#include <sw/client/common/commands.h>
#include <sw/client/common/main.h>

#include <primitives/sw/settings_program_name.h>
#include <primitives/sw/main.h>
#include <primitives/git_rev.h>

#if _MSC_VER
#if defined(SW_USE_TBBMALLOC)
#include "tbb/include/tbb/tbbmalloc_proxy.h"
#elif defined(SW_USE_TCMALLOC)
//"libtcmalloc_minimal.lib"
//#pragma comment(linker, "/include:__tcmalloc")
#else
#endif
#endif

#if _MSC_VER
#if defined(SW_USE_JEMALLOC)
#define JEMALLOC_NO_PRIVATE_NAMESPACE
#include <jemalloc-5.1.0/include/jemalloc/jemalloc.h>
//#include <jemalloc-5.1.0/src/jemalloc_cpp.cpp>
#endif
#endif

//#include <mimalloc.h>

int main(int argc, char **argv)
{
    //mi_version();
    //sw_enable_crash_server();

    StartupData sd(argc, argv);
    sd.program_short_name = "sw";
    return sd.run();
}

/*SUBCOMMAND_DECL(mirror)
{
    enum storage_file_type
    {
        SourceArchive,
        SpecificationFirstFile,
        About,
        BuildArchive, // binary archive?
    };
}*/

/*SUBCOMMAND_DECL(pack)
{
    // http://www.king-foo.com/2011/11/creating-debianubuntu-deb-packages/
    SW_UNIMPLEMENTED;
}*/

EXPORT_FROM_EXECUTABLE
std::string getVersionString()
{
    std::string s;
    s += ::sw::getProgramName();
    s += " version ";
    s += PACKAGE_VERSION;
    s += "\n";
    s += primitives::git_rev::getGitRevision();
    s += "\n";
    s += primitives::git_rev::getBuildTime();
    return s;
}

EXPORT_FROM_EXECUTABLE
std::string getProgramName()
{
    return PACKAGE_NAME_CLEAN;
}
