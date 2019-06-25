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
#include <sw/manager/settings.h>

extern ::cl::opt<String> build_arg_update;

static ::cl::opt<String> upload_remote(::cl::Positional, ::cl::desc("Remote name"), ::cl::sub(subcommand_upload));
String gUploadPrefix;
static ::cl::opt<String, true> upload_prefix(::cl::Positional, ::cl::desc("Prefix path"), ::cl::sub(subcommand_upload), ::cl::Required, ::cl::location(gUploadPrefix));
static ::cl::opt<bool> build_before_upload("build", ::cl::desc("Build before upload"), ::cl::sub(subcommand_upload));

sw::Remote *find_remote(sw::Settings &s, const String &name);

SUBCOMMAND_DECL(upload)
{
    auto swctx = createSwContext();
    cli_upload(*swctx);
}

SUBCOMMAND_DECL2(upload)
{
    // select remote first
    auto &us = sw::Settings::get_user_settings();
    auto current_remote = &*us.remotes.begin();
    if (!upload_remote.empty())
        current_remote = find_remote(us, upload_remote);

    sw::FetchOptions opts;
    //opts.name_prefix = upload_prefix;
    opts.dry_run = !build_before_upload;
    opts.root_dir = fs::current_path() / SW_BINARY_DIR;
    opts.ignore_existing_dirs = true;
    opts.existing_dirs_age = std::chrono::hours(8);
    //opts.apply_version_to_source = true;
    //swctx.
    auto s = sw::fetch_and_load(swctx, build_arg_update.getValue(), opts);
    if (build_before_upload)
    {
        s->execute();

        // after execution such solution has resolved deps and deps of the deps
        // we must not add them
        SW_UNIMPLEMENTED;
    }

    /*auto m = s->getPackages();
    // dbg purposes
    for (auto &[id, d] : m)
    {
        write_file(fs::current_path() / SW_BINARY_DIR / "upload" / id.toString() += ".json", d->getString());
        auto id2 = id;
        id2.ppath = PackagePath(upload_prefix) / id2.ppath;
        LOG_INFO(logger, "Uploading " + id2.toString());
    }

    // send signatures (gpg)
    // -k KEY1 -k KEY2
    auto api = current_remote->getApi();
    api->addVersion(upload_prefix, m, sw::read_config(build_arg_update.getValue()).value());*/
}
