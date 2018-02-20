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

#include <primitives/hash.h>

#define LOCAL_VERSION_NAME "local"

using ProjectId = uint64_t;
using ProjectVersionId = uint64_t;
using ProjectVersionNumber = int32_t;

enum class VersionType
{
    Any,
    Equal,
    Version,
    Branch,
};

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
    VersionType type{ VersionType::Any };

    Version(ProjectVersionNumber ma = -1, ProjectVersionNumber mi = -1, ProjectVersionNumber pa = -1);
    Version(const String &s);

    String toAnyVersion() const;
    String toString() const;
    path toPath() const;

    bool isValid() const;
    bool isBranch() const;
    bool isVersion() const;

    // checks if this version can be rhs using upgrade rules
    // does not check branches!
    // rhs should be exact version
    bool canBe(const Version &rhs) const;

    bool operator<(const Version &rhs) const;
    bool operator==(const Version &rhs) const;
    bool operator!=(const Version &rhs) const;

    static bool check_branch_name(const String &n, String *error = nullptr);
};

namespace std
{

template<> struct hash<Version>
{
    size_t operator()(const Version& v) const
    {
        if (!v.branch.empty())
            return std::hash<String>()(v.branch);
        size_t h = 0;
        hash_combine(h, v.major);
        hash_combine(h, v.minor);
        hash_combine(h, v.patch);
        return h;
    }
};

}
