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
