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
#include "http.h"

#include <functional>

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
