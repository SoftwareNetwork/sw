// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "driver.h"

#include <package.h>

namespace sw
{

SW_BUILDER_API
bool build(const path &file_or_dir);

SW_BUILDER_API
bool build(const Files &files_or_dirs);

SW_BUILDER_API
bool build(const PackageId &package);

SW_BUILDER_API
bool build(const PackagesIdSet &package);

SW_BUILDER_API
bool build(const String &file_or_dir_or_packagename);

SW_BUILDER_API
PackageScriptPtr build_only(const path &file_or_dir);

SW_BUILDER_API
PackageScriptPtr load(const path &file_or_dir);

SW_BUILDER_API
PackageScriptPtr fetch_and_load(const path &file_or_dir, const FetchOptions &opts = {});

SW_BUILDER_API
DriverPtr loadDriver(const path &file_or_dir);

SW_BUILDER_API
bool run(const PackageId &package);

SW_BUILDER_API
optional<String> read_config(const path &file_or_dir);

}
