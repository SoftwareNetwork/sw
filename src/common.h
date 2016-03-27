/*
 * C++ Archive Network Client
 * Copyright (C) 2016 Egor Pugin
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <map>
#include <memory>
#include <string>
#include <stdint.h>
#include <tuple>
#include <vector>

#include <openssl/evp.h>

#include "enums.h"
#include "filesystem.h"

#define CONFIG_ROOT "/etc/cppan/"
#define CPPAN_FILENAME "cppan.yml"

using String = std::string;
using Strings = std::vector<String>;

using ProjectVersionId = uint64_t;
using ProjectVersionNumber = int32_t;

std::tuple<int, String> system_with_output(const String &cmd);
std::tuple<int, String> system_with_output(const std::vector<String> &args);

bool check_login(const String &n, String *error = nullptr);
bool check_org_name(const String &n, String *error = nullptr);
bool check_project_name(const String &n, String *error = nullptr);
bool check_branch_name(const String &n, String *error = nullptr);
bool check_filename(const String &n, String *error = nullptr);

String make_archive_name(const String &fn);

path temp_directory_path();
path get_temp_filename();

struct DownloadData
{
    String url;
    path fn;
    int64_t file_size_limit = 1 * 1024 * 1024;
    String *dl_md5 = nullptr;
    std::ofstream *ofile = nullptr;

    DownloadData();
    ~DownloadData();

    void finalize();
    size_t progress(char *ptr, size_t size, size_t nmemb);

private:
    std::unique_ptr<EVP_MD_CTX> ctx;
};

void download_file(DownloadData &data);
void unpack_file(const path &fn, const path &dst);

String read_file(const path &p);

String generate_random_sequence(uint32_t len);
String hash_to_string(const uint8_t *hash, uint32_t hash_size);

String sha1(const String &data);

struct Version
{
    // undef gcc symbols
#ifdef major
#undef major
#endif
#ifdef minor
#undef minor
#endif
#ifdef patch
#undef patch
#endif

    ProjectVersionNumber major = -1;
    ProjectVersionNumber minor = -1;
    ProjectVersionNumber patch = -1;
    String branch;

    Version(ProjectVersionNumber ma = -1, ProjectVersionNumber mi = -1, ProjectVersionNumber pa = -1);
    Version(const String &s);

    String toAnyVersion() const;
    String toString() const;
    path toPath() const;

    bool isValid() const;
    bool isBranch() const { return !branch.empty(); }

    bool operator<(const Version &rhs) const;
};

Version get_program_version();
String get_program_version_string(const String &prog_name);
