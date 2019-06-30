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

}
