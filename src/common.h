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

#include <map>
#include <memory>
#include <set>
#include <string>
#include <stdint.h>
#include <tuple>
#include <vector>
#include <unordered_set>

#include <openssl/evp.h>

#include "enums.h"
#include "filesystem.h"
#include "property_tree.h"

#define CONFIG_ROOT "/etc/cppan/"
#define CPPAN_FILENAME "cppan.yml"

using String = std::string;
using Strings = std::vector<String>;

using FilesSorted = std::set<path>;
using Files = std::unordered_set<path>;

using ProjectVersionId = uint64_t;
using ProjectVersionNumber = int32_t;

std::tuple<int, String> system_with_output(const String &cmd);
std::tuple<int, String> system_with_output(const std::vector<String> &args);

bool check_branch_name(const String &n, String *error = nullptr);
bool check_filename(const String &n, String *error = nullptr);

int system_no_output(const String &cmd);
int system_no_output(const std::vector<String> &args);
int system(const std::vector<String> &args);

struct ProxySettings
{
    String host;
    String user;
};

String getAutoProxy();

struct HttpSettings
{
    bool verbose = false;
    bool ignore_ssl_checks = false;
    ProxySettings proxy;
};

extern HttpSettings httpSettings;

struct DownloadData
{
    String url;
    path fn;
    int64_t file_size_limit = 1 * 1024 * 1024;
    String *dl_md5 = nullptr;

    // service
    std::ofstream *ofile = nullptr;

    DownloadData();
    ~DownloadData();

    void finalize();
    size_t progress(char *ptr, size_t size, size_t nmemb);

private:
    std::unique_ptr<EVP_MD_CTX> ctx;
};

String url_post(const String &url, const String &data);
ptree url_post(const String &url, const ptree &data);
void download_file(DownloadData &data);
Files unpack_file(const path &fn, const path &dst);

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
    bool isVersion() const { return !isBranch(); }

    // checks if this version can be rhs using upgrade rules
    // does not check branches!
    // rhs should be exact version
    bool canBe(const Version &rhs) const;

    bool operator<(const Version &rhs) const;
    bool operator==(const Version &rhs) const;
    bool operator!=(const Version &rhs) const;
};

path get_program();
Version get_program_version();
String get_program_version_string(const String &prog_name);

std::wstring to_wstring(const std::string &s);
std::string to_string(const std::wstring &s);

String repeat(const String &e, int n);

// lambda overloads
template <class... Fs> struct overload_set;

template <class F1, class... Fs>
struct overload_set<F1, Fs...> : F1, overload_set<Fs...>::type
{
    typedef overload_set type;

    overload_set(F1 head, Fs... tail)
        : F1(head), overload_set<Fs...>::type(tail...)
    {}

    using F1::operator();
    using overload_set<Fs...>::type::operator();
};

template <class F>
struct overload_set<F> : F
{
    typedef F type;
    using F::operator();
};

template <class... Fs>
typename overload_set<Fs...>::type overload(Fs... x)
{
    return overload_set<Fs...>(x...);
}
