// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <primitives/filesystem.h>

#include <unordered_map>

// replace with .swb? .sw.b? .swbin? .swbuild?
// so we leave .sw for some misc but important files to include into repositories
#define SW_BINARY_DIR ".sw"

namespace sw::support
{

SW_SUPPORT_API
path get_root_directory();

SW_SUPPORT_API
path get_config_filename();

SW_SUPPORT_API
path temp_directory_path(const path &subdir = path());

SW_SUPPORT_API
path get_temp_filename(const path &subdir = path());

SW_SUPPORT_API
path get_ca_certs_filename();

SW_SUPPORT_API
String make_archive_name(const String &fn = String());

SW_SUPPORT_API
path findRootDirectory(const path &p);

// cached version
SW_SUPPORT_API
void create_directories(const path &p);

// will not shrink if old limit is lower
// return old limit?
SW_SUPPORT_API
int set_max_open_files_limit(int newlimit);

}
