// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
