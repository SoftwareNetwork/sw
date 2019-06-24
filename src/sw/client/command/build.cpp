// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

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

    swctx.build((Strings&)build_arg);
}
