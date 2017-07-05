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
#include "filesystem.h"
#include "property_tree.h"
#include "yaml.h"

#include <boost/variant.hpp>

#include <set>

struct Git
{
    String url;
    String tag;
    String branch;
    String commit;

    bool empty() const { return url.empty(); }
    bool isValid(String *error = nullptr) const;
    bool operator==(const Git &rhs) const
    {
        return std::tie(url, tag, branch, commit) == std::tie(rhs.url, rhs.tag, rhs.branch, rhs.commit);
    }
};

struct Hg : Git
{
    int64_t revision = -1;

    bool isValid(String *error = nullptr) const;
    bool operator==(const Hg &rhs) const
    {
        return std::tie(url, tag, branch, commit, revision) == std::tie(rhs.url, rhs.tag, rhs.branch, rhs.commit, rhs.revision);
    }
};

struct Bzr
{
    String url;
    String tag;
    int64_t revision = -1;

    bool empty() const { return url.empty(); }
    bool isValid(String *error = nullptr) const;
    bool operator==(const Bzr &rhs) const
    {
        return std::tie(url, tag, revision) == std::tie(rhs.url, rhs.tag, rhs.revision);
    }
};

struct Fossil : Git
{
    bool isValid(String *error = nullptr) const;
    bool operator==(const Fossil &rhs) const
    {
        return std::tie(url, tag, branch, commit) == std::tie(rhs.url, rhs.tag, rhs.branch, rhs.commit);
    }
};

struct RemoteFile
{
    String url;

    bool operator==(const RemoteFile &rhs) const
    {
        return std::tie(url) == std::tie(rhs.url);
    }
};

struct RemoteFiles
{
    std::set<String> urls;

    bool operator==(const RemoteFiles &rhs) const
    {
        return std::tie(urls) == std::tie(rhs.urls);
    }
};

// add svn, p4, cvs, darcs
// do not add local files
using Source = boost::variant<Git, Hg, Bzr, Fossil, RemoteFile, RemoteFiles>;

struct DownloadSource
{
    int64_t max_file_size = 0;

    void operator()(const Git &git);
    void operator()(const Hg &hg);
    void operator()(const Bzr &bzr);
    void operator()(const Fossil &fossil);
    void operator()(const RemoteFile &rf);
    void operator()(const RemoteFiles &rfs);

    void download(const Source &source);

private:
    void download_file(const String &url, const path &fn);
    void download_and_unpack(const String &url, const path &fn);
};

bool load_source(const yaml &root, Source &source);
void save_source(yaml &root, const Source &source);

Source load_source(const ptree &p);
void save_source(ptree &p, const Source &source);

String print_source(const Source &source);

bool isValidSourceUrl(const Source &source);

void run(const String &c);
