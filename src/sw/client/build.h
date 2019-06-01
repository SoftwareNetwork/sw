// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <sw/manager/source.h>

#include <primitives/filesystem.h>

namespace sw
{

struct Build;
struct PackageId;
struct SwContext;

std::optional<String> read_config(const path &file_or_dir);
std::unique_ptr<Build> load(const SwContext &swctx, const path &file_or_dir);
void run(const SwContext &swctx, const PackageId &package);

struct FetchOptions : SourceDownloadOptions
{
    String name_prefix;
    bool apply_version_to_source = false;
    bool dry_run = true;
    bool parallel = true;
};

std::unique_ptr<Build> fetch_and_load(const SwContext &swctx, const path &file_or_dir, const FetchOptions &opts = {});

}
