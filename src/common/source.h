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

// add svn, bzr, hg?
// do not add local files
using Source = boost::variant<Git, RemoteFile, RemoteFiles>;

struct DownloadSource
{
    int64_t max_file_size = 0;

    void operator()(const Git &git);
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
