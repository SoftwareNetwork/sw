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

#include "common.h"

#include "yaml.h"

#include <boost/variant.hpp>

#include <set>

struct Git
{
    String url;
    String tag;
    String branch;

    bool empty() const { return url.empty(); }

    bool isValid(String *error = nullptr) const
    {
        if (empty())
        {
            if (error)
                *error = "Git url is missing";
            return false;
        }
        if (tag.empty() && branch.empty())
        {
            if (error)
                *error = "No git sources (branch or tag) available";
            return false;
        }
        if (!tag.empty() && !branch.empty())
        {
            if (error)
                *error = "Only one git source (branch or tag) must be specified";
            return false;
        }
        return true;
    }
};

struct RemoteFile { String url; };
struct RemoteFiles { std::set<String> urls; };

// add svn, bzr, hg?
// do not add local files
using Source = boost::variant<Git, RemoteFile, RemoteFiles>;

bool load_source(const yaml &root, Source &source);
void save_source(yaml &root, const Source &source);

struct DownloadSource
{
    path root_dir;
    int64_t max_file_size = 0;

    void operator()(const Git &git);
    void operator()(const RemoteFile &rf);
    void operator()(const RemoteFiles &rfs);

    void download(const Source &source);

private:
    void download_file(const String &url, const path &fn);
    void download_and_unpack(const String &url, const path &fn);
};
