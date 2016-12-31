/*
 * Copyright (c) 2016, Egor Pugin
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     1. Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *     3. Neither the name of the copyright holder nor the names of
 *        its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
