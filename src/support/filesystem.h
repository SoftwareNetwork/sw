// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <primitives/filesystem.h>

#include <unordered_map>

#define STAMPS_DIR "stamps"
#define STORAGE_DIR "storage"

// replace with .swb? .sw.b? .swbin? .swbuild?
// so we leave .sw for some misc but important files to include into repositories
#define SW_BINARY_DIR ".sw"

using Stamps = std::unordered_map<path, time_t>;
using SourceGroups = std::map<String, std::set<String>>;

SW_SUPPORT_API
path get_root_directory();

SW_SUPPORT_API
path get_config_filename();

SW_SUPPORT_API
path temp_directory_path(const path &subdir = path());

SW_SUPPORT_API
path get_temp_filename(const path &subdir = path());

SW_SUPPORT_API
String make_archive_name(const String &fn = String());

SW_SUPPORT_API
path findRootDirectory(const path &p);

// cached version
SW_SUPPORT_API
void create_directories(const path &p);
