// Copyright (C) 2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <boost/algorithm/string.hpp>
#include <primitives/command.h>
#include <primitives/sw/main.h>
#include <primitives/sw/cl.h>
#include <primitives/sw/settings_program_name.h>

static cl::opt<path> git(cl::Positional, cl::Required);
static cl::opt<path> wdir(cl::Positional, cl::Required);
static cl::opt<path> outfn(cl::Positional, cl::Required);

int main(int argc, char **argv)
{
    cl::ParseCommandLineOptions(argc, argv);
    String rev, status, time;

    {
        primitives::Command c;
        c.working_directory = wdir;
        c.arguments.push_back(git.u8string());
        c.arguments.push_back("rev-parse");
        c.arguments.push_back("HEAD");
        c.execute();
        rev = boost::trim_copy(c.out.text);
    }

    {
        primitives::Command c;
        c.working_directory = wdir;
        c.arguments.push_back(git.u8string());
        c.arguments.push_back("status");
        c.arguments.push_back("--porcelain");
        c.arguments.push_back("-uno");
        c.execute();
        status = boost::trim_copy(c.out.text);
        if (status.empty())
            status = "0";
        else
            status = std::to_string(split_lines(status).size());
    }

    {
        time = std::to_string(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    }

    String t;
    t += "#define SW_GIT_REV \"" + rev + "\"\n";
    t += "#define SW_GIT_CHANGED_FILES " + status + "\n";
    t += "#define SW_BUILD_TIME_T " + time + "LL\n";

    write_file(outfn, t);

    return 0;
}
