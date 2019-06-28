/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2019 Egor Pugin
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

#include "commands.h"

extern ::cl::opt<bool> build_after_fetch;

::cl::list<String> build_arg(::cl::Positional, ::cl::desc("Files or directories to build (paths to config)"), ::cl::sub(subcommand_build));

static ::cl::opt<String> build_source_dir("S", ::cl::desc("Explicitly specify a source directory."), ::cl::sub(subcommand_build), ::cl::init("."));
static ::cl::opt<String> build_binary_dir("B", ::cl::desc("Explicitly specify a build directory."), ::cl::sub(subcommand_build), ::cl::init(SW_BINARY_DIR));

static ::cl::opt<bool> build_fetch("fetch", ::cl::desc("Fetch sources, then build"), ::cl::sub(subcommand_build));

SUBCOMMAND_DECL(build)
{
    auto swctx = createSwContext();
    cli_build(*swctx);
}

SUBCOMMAND_DECL2(build)
{
    if (build_fetch)
    {
        build_after_fetch = true;
        return cli_fetch(swctx);
    }

    // defaults or only one of build_arg and -S specified
    //  -S == build_arg
    //  -B == fs::current_path()

    // if -S and build_arg specified:
    //  source dir is taken as -S, config dir is taken as build_arg

    // if -B specified, it is used as is

    for (auto &a : build_arg)
        swctx.addInput(a);
    swctx.build();
}
