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

#include "cppan_string.h"

#include <boost/filesystem.hpp>
#include <boost/functional/hash.hpp>
#include <boost/range.hpp>

#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#define STAMPS_DIR "stamps"
#define STORAGE_DIR "storage"
#define CPPAN_FILENAME "cppan.yml"

namespace fs = boost::filesystem;
using path = fs::wpath;

using FilesSorted = std::set<path>;
using Files = std::unordered_set<path>;

using Stamps = std::unordered_map<path, time_t>;
using SourceGroups = std::map<String, std::set<String>>;

path get_home_directory();
path get_root_directory();
path get_config_filename();

path temp_directory_path(const path &subdir = path());
path get_temp_filename(const path &subdir = path());

String read_file(const path &p, bool no_size_check = false);
void write_file(const path &p, const String &s);
void write_file_if_different(const path &p, const String &s);
std::vector<String> read_lines(const path &p);

void remove_file(const path &p);
String normalize_path(const path &p);
bool is_under_root(path p, const path &root_dir);

String get_stamp_filename(const String &prefix);
String make_archive_name(const String &fn = String());

void copy_dir(const path &source, const path &destination);
void remove_files_like(const path &dir, const String &regex);

bool compare_files(const path &fn1, const path &fn2);
bool compare_dirs(const path &dir1, const path &dir2);

path findRootDirectory(const path &p = fs::current_path());

namespace std
{
    template<> struct hash<path>
    {
        size_t operator()(const path& p) const
        {
            return fs::hash_value(p);
        }
    };
}

class ScopedCurrentPath
{
public:
    ScopedCurrentPath()
    {
        old = fs::current_path();
        cwd = old;
    }
    ScopedCurrentPath(const path &p)
        : ScopedCurrentPath()
    {
        if (!p.empty())
        {
            fs::current_path(p);
            // abs path, not possibly relative p
            cwd = fs::current_path();
        }
    }
    ~ScopedCurrentPath()
    {
        return_back();
    }

    void return_back()
    {
        if (!active)
            return;
        fs::current_path(old);
        cwd = old;
        active = false;
    }

    path get_cwd() const { return cwd; }

private:
    path old;
    path cwd;
    bool active = true;
};
