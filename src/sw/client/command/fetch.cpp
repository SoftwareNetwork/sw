// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "commands.h"
#include "../build.h"

#include <sw/driver/build.h>

::cl::opt<bool> build_after_fetch("build", ::cl::desc("Build after fetch"), ::cl::sub(subcommand_fetch));

SUBCOMMAND_DECL(fetch)
{
    auto swctx = createSwContext();
    cli_fetch(*swctx);
}

SUBCOMMAND_DECL2(fetch)
{
    sw::FetchOptions opts;
    //opts.name_prefix = upload_prefix;
    opts.dry_run = !build_after_fetch;
    opts.root_dir = fs::current_path() / SW_BINARY_DIR;
    opts.ignore_existing_dirs = true;
    opts.existing_dirs_age = std::chrono::hours(1);
    //opts.apply_version_to_source = true;
    auto s = sw::fetch_and_load(swctx, ".", opts);
    if (build_after_fetch)
        s->execute();
}
