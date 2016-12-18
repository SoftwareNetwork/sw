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
#include "http.h"

#define DEFAULT_REMOTE_NAME "origin"

struct Package;

String default_source_provider(const Package &);

struct Remote
{
    using Url = String;
    using SourcesUrls = std::vector<Url>;
    using SourceUrlProvider = std::function<String(const Remote &, const Package &)>;

    String name;

    Url url;
    String data_dir;

    String user;
    String token;

    // own data
    // sources
    std::vector<SourceUrlProvider> primary_sources;
    SourceUrlProvider default_source{ &Remote::default_source_provider };
    std::vector<SourceUrlProvider> additional_sources;

    bool downloadPackage(const Package &d, const String &hash, const path &fn, bool try_only_first = false) const;

public:
    String default_source_provider(const Package &) const;
    String github_source_provider(const Package &) const;
};

using Remotes = std::vector<Remote>;
Remotes get_default_remotes();
