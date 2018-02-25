/*
 * Copyright (C) 2016-2017, Egor Pugin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <primitives/filesystem.h>

#include <unordered_map>

#define STAMPS_DIR "stamps"
#define STORAGE_DIR "storage"
#define CPPAN_FILENAME "cppan.yml"

using Stamps = std::unordered_map<path, time_t>;
using SourceGroups = std::map<String, std::set<String>>;

path get_root_directory();
path get_config_filename();

path temp_directory_path(const path &subdir = path());
path get_temp_filename(const path &subdir = path());

String get_stamp_filename(const String &prefix);
String make_archive_name(const String &fn = String());

path findRootDirectory(const path &p);
