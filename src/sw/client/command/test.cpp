// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "commands.h"

extern bool gWithTesting;
extern ::cl::list<String> build_arg;

::cl::list<String> build_arg_test(::cl::Positional, ::cl::desc("File or directory to use to generate projects"), ::cl::sub(subcommand_test));

SUBCOMMAND_DECL(test)
{
    auto swctx = createSwContext();
    gWithTesting = true;
    (Strings&)build_arg = (Strings&)build_arg_test;
    cli_build(*swctx);
}
