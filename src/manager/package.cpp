// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "dependency.h"

#include "database.h"
#include "directories.h"
#include "hash.h"
#include "lock.h"
#include "resolver.h"

#include <primitives/sw/settings.h>

#include <boost/algorithm/string.hpp>

#include <fstream>
#include <iostream>
#include <regex>
#include <shared_mutex>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "package");

static cl::opt<bool> separate_bdir("separate-bdir", cl::init(true));

namespace sw
{

UnresolvedPackage::UnresolvedPackage(const String &s)
{
    *this = extractFromString(s);
}

UnresolvedPackage::UnresolvedPackage(const PackagePath &p, const VersionRange &r)
{
    ppath = p;
    range = r;
}

String UnresolvedPackage::toString() const
{
    return ppath.toString() + "-" + range.toString();
}

bool UnresolvedPackage::canBe(const PackageId &id) const
{
    return ppath == id.ppath && range.hasVersion(id.version);
}

ExtendedPackageData UnresolvedPackage::resolve()
{
    return resolve_dependencies({*this})[*this];

    /*if (auto v = range.getMaxSatisfyingVersion(); v)
        return { ppath, v.value() };
    throw std::runtime_error("Cannot resolve package: " + toString());*/
}

PackageId::PackageId(const String &target)
{
    auto pos = target.find('-');
    if (pos == target.npos)
        ppath = target;
    else
    {
        ppath = target.substr(0, pos);
        version = target.substr(pos + 1);
    }
}

PackageId::PackageId(const PackagePath &p, const Version &v)
    : ppath(p), version(v)
{
}

path PackageId::getDir() const
{
    return getDir(getUserDirectories().storage_dir_pkg);
}

path PackageId::getDir(const path &p) const
{
    return p / getHashPath();
}

path PackageId::getDirSrc() const
{
    return getDir(getUserDirectories().storage_dir_pkg) / "src";
}

path PackageId::getDirSrc2() const
{
    auto &pkgs = getServiceDatabase().getOverriddenPackages();
    auto i = pkgs.find(*this);
    if (i == pkgs.end())
        return getDirSrc() / SW_SDIR_NAME;
    return i->second;
}

path PackageId::getDirObj() const
{
    if (!separate_bdir)
        return getDir(getUserDirectories().storage_dir_pkg) / "obj";
    return getDir(getUserDirectories().storage_dir_obj) / "obj";
}

path PackageId::getStampFilename() const
{
    auto b = getUserDirectories().storage_dir_etc / STAMPS_DIR / "packages" / getHashPath();
    auto f = b.filename();
    b = b.parent_path();
    b /= get_stamp_filename(f.string());
    return b;
}

String PackageId::getStampHash() const
{
    String hash;
    std::ifstream ifile(getStampFilename());
    if (ifile)
        ifile >> hash;
    return hash;
}

String PackageId::getHash() const
{
    static const auto delim = "-";
    //if (hash.empty())
        return blake2b_512(ppath.toStringLower() + delim + version.toString());
    //return hash;
}

String PackageId::getHashShort() const
{
    return shorten_hash(getHash());
}

String PackageId::getFilesystemHash() const
{
    return getHashShort();
}

#define N_SUBDIRS 4
#define N_CHARS_PER_SUBDIR 2

path PackageId::getHashPath() const
{
    return getHashPathFromHash(getFilesystemHash());
}

path PackageId::getHashPathSha256() const
{
    static const auto delim = "/";
    auto h = sha256(ppath.toStringLower() + delim + version.toString());
    return getHashPathFromHash(shorten_hash(h));
}

path PackageId::getHashPathFull() const
{
    return getHashPathFromHash(getHash());
}

path PackageId::getHashPathFromHash(const String &h)
{
    path p;
    int i = 0;
    for (; i < N_SUBDIRS; i++)
        p /= h.substr(i * N_CHARS_PER_SUBDIR, N_CHARS_PER_SUBDIR);
    p /= h.substr(i * N_CHARS_PER_SUBDIR);
    return p;
}

void PackageId::createNames()
{
    auto v = version.toString();

    target_name = ppath.toString() + (v == "*" ? "" : ("-" + v));

    // for local projects we use simplified variable name without
    // the second dir hash argument
    auto vname = ppath.toString();
    if (ppath.is_loc())
        vname = ppath[PackagePath::ElementType::Namespace] / ppath[PackagePath::ElementType::Tail];

    variable_name = vname + (v == "*" ? "" : ("_" + v));
    std::replace(variable_name.begin(), variable_name.end(), '.', '_');

    variable_no_version_name = vname;
    std::replace(variable_no_version_name.begin(), variable_no_version_name.end(), '.', '_');

    hash = getHash();
    target_name_hash = getHashShort();
}

String PackageId::getTargetName() const
{
    if (target_name.empty())
    {
        auto v = version.toString();
        return ppath.toString() + (v == "*" ? "" : ("-" + v));
    }
    return target_name;
}

String PackageId::getVariableName() const
{
    if (variable_name.empty())
    {
        auto v = version.toString();
        auto vname = ppath.toString() + "_" + (v == "*" ? "" : ("_" + v));
        std::replace(vname.begin(), vname.end(), '.', '_');
        return vname;
    }
    return variable_name;
}

bool PackageId::canBe(const PackageId &rhs) const
{
    return ppath == rhs.ppath/* && version.canBe(rhs.version)*/;
}

Package PackageId::toPackage() const
{
    Package p;
    p.ppath = ppath;
    p.version = version;
    return p;
}

String PackageId::toString() const
{
    return ppath.toString() + "-" + version.toString();
}

PackageId extractFromStringPackageId(const String &target)
{
    auto pos = target.find('-');

    PackageId p;
    if (pos == target.npos)
        throw std::runtime_error("Bad target");
    else
    {
        p.ppath = target.substr(0, pos);
        p.version = target.substr(pos + 1);
    }
    return p;
}

UnresolvedPackage extractFromString(const String &target)
{
    auto pos = target.find('-');

    UnresolvedPackage p;
    if (pos == target.npos)
        p.ppath = target;
    else
    {
        p.ppath = target.substr(0, pos);
        p.range = target.substr(pos + 1);
    }
    return p;
}

void cleanPackages(const String &s, int flags)
{
    // on source flag remove all
    if (flags & CleanTarget::Src)
        flags = CleanTarget::All;

    std::regex r(s);
    Packages pkgs;

    // find direct packages
    auto &sdb = getServiceDatabase();
    auto ipkgs = sdb.getInstalledPackages();
    for (auto &pkg : ipkgs)
    {
        if (!std::regex_match(pkg.target_name, r))
            continue;
        pkgs.insert(pkg);
    }

    if (pkgs.empty())
        return;

    // find dependent packages and remove non installed
    auto dpkgs = getPackagesDatabase().getTransitiveDependentPackages(pkgs);
    for (auto i = dpkgs.begin(); i != dpkgs.end();)
    {
        if (ipkgs.find(*i) == ipkgs.end())
            i = dpkgs.erase(i);
        else
            ++i;
    }

    cleanPackages(pkgs, flags);

    if (flags & CleanTarget::Src)
    {
        // dependent packages must be rebuilt but with only limited set of the flags
        flags = CleanTarget::Bin |
            CleanTarget::Lib |
            CleanTarget::Obj |
            CleanTarget::Exp;
    }

    cleanPackages(dpkgs, flags);
}

void cleanPackage(const PackageId &pkg, int flags)
{
    static std::map<PackageId, int> cleaned_packages;
    static std::shared_mutex m;

    static const auto cache_dir_bin = enumerate_files(getDirectories().storage_dir_bin);
    //static const auto cache_dir_exp = enumerate_files(getDirectories().storage_dir_exp);
    static const auto cache_dir_lib = enumerate_files(getDirectories().storage_dir_lib);
#ifdef _WIN32
    //static const auto cache_dir_lnk = enumerate_files(getDirectories().storage_dir_lnk);
#endif

    // only clean yet uncleaned flags
    {
        std::shared_lock<std::shared_mutex> lock(m);
        auto i = cleaned_packages.find(pkg);
        if (i != cleaned_packages.end())
            flags = flags & ~i->second;
    }

    if (flags == 0)
        return;

    // log message
    {
        String s;
        if (flags != CleanTarget::All)
        {
            s += " (";
            auto fs = CleanTarget::getStringsById();
            for (auto &f : fs)
            {
                if (flags & f.first)
                    s += f.second + ", ";
            }
            if (s.size() > 2)
                s.resize(s.size() - 2);
            s += ")";
        }
        LOG_INFO(logger, "Cleaning   : " + pkg.target_name + "..." + s);
    }

    auto rm = [](const auto &p)
    {
        if (fs::exists(p))
        {
            error_code ec;
            fs::remove_all(p, ec);
        }
    };

    auto rm_recursive = [](const auto &pkg, const auto &files, const auto &ext)
    {
        error_code ec;
        for (auto &f : files)
        {
            auto fn = f.filename().string();
            if (fn == pkg.target_name + ext)
                fs::remove(f, ec);
        }
    };

    if (flags & CleanTarget::Src)
        rm(pkg.getDirSrc());
    if (flags & CleanTarget::Obj)
        rm(pkg.getDirObj() / "build"); // for object targets we remove subdir

    if (flags & CleanTarget::Bin)
        remove_files_like(cache_dir_bin, ".*" + pkg.target_name + ".*");
    if (flags & CleanTarget::Lib)
        remove_files_like(cache_dir_lib, ".*" + pkg.target_name + ".*");

    // cmake exports
    //if (flags & CleanTarget::Exp)
        //rm_recursive(pkg, cache_dir_exp, ".cmake");

#ifdef _WIN32
    // solution links
    //if (flags & CleanTarget::Lnk)
        //rm_recursive(pkg, cache_dir_lnk, ".sln.lnk");
#endif

    // remove packages at the end in case we're removing sources
    if (flags & CleanTarget::Src)
    {
        auto &sdb = getServiceDatabase();
        sdb.removeInstalledPackage(pkg);
    }

    // save cleaned packages
    {
        std::lock_guard<std::shared_mutex> lock(m);
        cleaned_packages[pkg] |= flags;
    }
}

void cleanPackages(const Packages &pkgs, int flags)
{
    for (auto &pkg : pkgs)
        cleanPackage(pkg, flags);
}

std::unordered_map<int, String> CleanTarget::getStringsById()
{
    static std::unordered_map<int, String> m
    {
#define ADD(x) { CleanTarget::x, boost::to_lower_copy(String(#x)) }

        ADD(Src),
        ADD(Obj),
        ADD(Lib),
        ADD(Bin),
        ADD(Exp),
        ADD(Lnk),

#undef ADD
    };
    return m;
}

std::unordered_map<String, int> CleanTarget::getStrings()
{
    auto m = CleanTarget::getStringsById();
    std::unordered_map<String, int> m2;
    for (auto &s : m)
        m2[s.second] = s.first;
    return m2;
}

}
