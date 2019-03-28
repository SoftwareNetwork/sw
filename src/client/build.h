// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <source.h>

#include <primitives/filesystem.h>

namespace sw
{

struct PackageId;
struct Build;

void build(const path &file_or_dir);
void build(const Files &files_or_dirs);
void build(const String &file_or_dir_or_packagename);
void build(const String &pkg);
void build(const Strings &packages);

std::optional<String> read_config(const path &file_or_dir);
std::unique_ptr<Build> load(const path &file_or_dir);
void run(const PackageId &package);

struct FetchOptions : SourceDownloadOptions
{
    String name_prefix;
    bool apply_version_to_source = false;
    bool dry_run = true;
    bool parallel = true;
};

std::unique_ptr<Build> fetch_and_load(const path &file_or_dir, const FetchOptions &opts = {});

}
