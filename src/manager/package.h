// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "enums.h"
#include "package_path.h"
#include "cppan_version.h"
#include "source.h"

#include <unordered_set>
#include <unordered_map>

#define SW_SDIR_NAME "sdir"
#define SW_BDIR_NAME "bdir"
#define SW_BDIR_PRIVATE_NAME "bdir_pvt"

namespace sw
{

struct PackageId;
struct Package;
struct ExtendedPackageData;

struct SW_MANAGER_API UnresolvedPackage
{
    PackagePath ppath;
    VersionRange range;

    UnresolvedPackage() = default;
    UnresolvedPackage(const PackagePath &p, const VersionRange &r);
    UnresolvedPackage(const String &s);

    String toString() const;
    bool canBe(const PackageId &id) const;

    /// return max satisfying package id
    ExtendedPackageData resolve();

    bool operator<(const UnresolvedPackage &rhs) const { return std::tie(ppath, range) < std::tie(rhs.ppath, rhs.range); }
    bool operator==(const UnresolvedPackage &rhs) const { return std::tie(ppath, range) == std::tie(rhs.ppath, rhs.range); }
    bool operator!=(const UnresolvedPackage &rhs) const { return !operator==(rhs); }
};

using UnresolvedPackages = std::unordered_set<UnresolvedPackage>;

struct SW_MANAGER_API PackageDescriptionInternal
{
    virtual ~PackageDescriptionInternal() = default;
    virtual std::tuple<path /* root dir */, Files> getFiles() const = 0;
    virtual UnresolvedPackages getDependencies() const = 0;

    // source
    // icons
    // screenshots, previews
    // desc: type, summary,
};

struct SW_MANAGER_API PackageId
{
    PackagePath ppath;
    Version version;

    PackageId() = default;
    // try to extract from string
    PackageId(const String &);
    PackageId(const PackagePath &, const Version &);
    /*PackageId(const PackageId &) = default;
    PackageId &operator=(const PackageId &) = default;*/

    PackagePath getPath() const { return ppath; }
    Version getVersion() const { return version; }

    path getDir() const;
    path getDirSrc() const;
    path getDirSrc2() const;
    path getDirObj() const;
    path getDirObjWdir() const;
    path getDirInfo() const;
    optional<path> getOverriddenDir() const;
    String getHash() const;
    String getHashShort() const;
    String getFilesystemHash() const;
    path getHashPath() const;
    path getHashPathSha256() const; // old, compat
    path getHashPathFull() const;
    path getStampFilename() const;
    String getStampHash() const;

    bool canBe(const PackageId &rhs) const;

    // delete?
    bool empty() const { return ppath.empty()/* || !version.isValid()*/; }

    bool operator<(const PackageId &rhs) const { return std::tie(ppath, version) < std::tie(rhs.ppath, rhs.version); }
    bool operator==(const PackageId &rhs) const { return std::tie(ppath, version) == std::tie(rhs.ppath, rhs.version); }
    bool operator!=(const PackageId &rhs) const { return !operator==(rhs); }

    // misc data
    String target_name;
    String target_name_hash;
    String variable_name;
    String variable_no_version_name;

    void createNames();
    String getTargetName() const;
    String getVariableName() const;

    Package toPackage() const;
    String toString() const;

    bool isPublic() const { return !isPrivate(); }
    bool isPrivate() const { return ppath.is_pvt() || ppath.is_com(); }

private:
    // cached vars
    String hash;

    path getDir(const path &p) const;
    static path getHashPathFromHash(const String &h);
};

//using PackagesId = std::unordered_map<String, PackageId>;
//using PackagesIdMap = std::unordered_map<PackageId, PackageId>;
using PackagesIdSet = std::unordered_set<PackageId>;

SW_MANAGER_API
UnresolvedPackage extractFromString(const String &target);

SW_MANAGER_API
PackageId extractFromStringPackageId(const String &target);

struct SW_MANAGER_API Package : PackageId
{
    SomeFlags flags;

    /*using PackageId::PackageId;
    using PackageId::operator=;
    Package(const PackageId &p) : PackageId(p) {}*/
};

using Packages = std::unordered_set<Package>;

struct CleanTarget
{
    enum Type
    {
        None = 0b0000'0000,

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
void cleanPackages(const Packages &pkgs, int flags);

}

namespace std
{

template<> struct hash<sw::PackageId>
{
    size_t operator()(const sw::PackageId &p) const
    {
        auto h = std::hash<sw::PackagePath>()(p.ppath);
        return hash_combine(h, std::hash<sw::Version>()(p.version));
    }
};

template<> struct hash<sw::Package>
{
    size_t operator()(const sw::Package &p) const
    {
        return std::hash<sw::PackageId>()(p);
    }
};

template<> struct hash<sw::UnresolvedPackage>
{
    size_t operator()(const sw::UnresolvedPackage &p) const
    {
        auto h = std::hash<sw::PackagePath>()(p.ppath);
        return hash_combine(h, std::hash<sw::VersionRange>()(p.range));
    }
};

}
