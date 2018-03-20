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
#include "enums.h"
#include "filesystem.h"
#include "project_path.h"
#include "version.h"

#include <primitives/hash.h>

#include <map>

struct Package
{
    ProjectPath ppath;
    Version version;
    ProjectFlags flags;

    // extended data
    // probably can be moved to child struct Dependency
    String reference;
    StringSet conditions;

    path getDirSrc() const;
    path getDirObj() const;
    String getHash() const;
    String getHashShort() const;
    String getFilesystemHash() const;
    path getHashPath() const;
    path getStampFilename() const;
    String getStampHash() const;

    bool empty() const { return ppath.empty() || !version.isValid(); }
    bool operator<(const Package &rhs) const { return std::tie(ppath, version) < std::tie(rhs.ppath, rhs.version); }
    bool operator==(const Package &rhs) const { return std::tie(ppath, version) == std::tie(rhs.ppath, rhs.version); }
    bool operator!=(const Package &rhs) const { return !operator==(rhs); }

    // misc data
    String target_name;
    String target_name_hash;
    String variable_name;
    String variable_no_version_name;

    void createNames();
    String getTargetName() const;
    String getVariableName() const;

private:
    // cached vars
    String hash;

    path getDir(const path &p) const;
};

using Packages = std::unordered_map<String, Package>;
using PackagesMap = std::unordered_map<Package, Package>;
using PackagesSet = std::unordered_set<Package>;

Package extractFromString(const String &target);
Package extractFromStringAny(const String &target);

struct CleanTarget
{
    enum Type
    {
        None= 0b0000'0000,

        Src = 0b0000'0001,
        Obj = 0b0000'0010,
        Lib = 0b0000'0100,
        Bin = 0b0000'1000,
        Exp = 0b0001'0000,
        Lnk = 0b0010'0000,

        All = 0xFF,
        AllExceptSrc = All & ~Src,
    };

    static std::unordered_map<String, int> getStrings();
    static std::unordered_map<int, String> getStringsById();
};

void cleanPackages(const String &s, int flags = CleanTarget::All);
void cleanPackages(const PackagesSet &pkgs, int flags);

namespace std
{

template<> struct hash<Package>
{
    size_t operator()(const Package& p) const
    {
        auto h = std::hash<ProjectPath>()(p.ppath);
        return hash_combine(h, std::hash<Version>()(p.version));
    }
};

}
