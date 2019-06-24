// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "commands.h"

extern ::cl::opt<bool> dry_run;
extern ::cl::list<String> build_arg;

::cl::opt<String> build_arg_update(::cl::Positional, ::cl::desc("Update lock"), ::cl::init("."), ::cl::sub(subcommand_update));

SUBCOMMAND_DECL(update)
{
    SW_UNIMPLEMENTED;

    auto swctx = createSwContext();
    dry_run = true;
    ((Strings&)build_arg).clear();
    build_arg.push_back(build_arg_update.getValue());
    cli_build(*swctx);
}
