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

#include "dependency.h"

#include "config.h"
#include "database.h"
#include "directories.h"
#include "hash.h"
#include "lock.h"

#include <boost/algorithm/string.hpp>
#include <boost/nowide/fstream.hpp>

#include <regex>
#include <shared_mutex>

#include <primitives/log.h>
//DECLARE_STATIC_LOGGER(logger, "package");

path Package::getDir(const path &p) const
{
    return p / getHashPath();
}

path Package::getDirSrc() const
{
    return getDir(directories.storage_dir_src);
}

path Package::getDirObj() const
{
    return getDir(directories.storage_dir_obj);
}

path Package::getStampFilename() const
{
    auto b = directories.storage_dir_etc / STAMPS_DIR / "packages" / getHashPath();
    auto f = b.filename();
    b = b.parent_path();
    b /= get_stamp_filename(f.string());
    return b;
}

String Package::getStampHash() const
{
    String hash;
    boost::nowide::ifstream ifile(getStampFilename().string());
    if (ifile)
        ifile >> hash;
    return hash;
}

String Package::getHash() const
{
    static const auto delim = "/";
    if (hash.empty())
        return sha256(ppath.toString() + delim + version.toString());
    return hash;
}

String Package::getHashShort() const
{
    return shorten_hash(getHash());
}

String Package::getFilesystemHash() const
{
    return getHashShort();
}

path Package::getHashPath() const
{
    auto h = getFilesystemHash();
    path p;
    p /= h.substr(0, 2);
    p /= h.substr(2, 2);
    p /= h.substr(4);
    return p;
}

void Package::createNames()
{
    auto v = version.toAnyVersion();

    target_name = ppath.toString() + (v == "*" ? "" : ("-" + v));

    // for local projects we use simplified variable name without
    // the second dir hash argument
    auto vname = ppath.toString();
    if (ppath.is_loc())
        vname = ppath[PathElementType::Namespace] / ppath[PathElementType::Tail];

    variable_name = vname + (v == "*" ? "" : ("_" + v));
    std::replace(variable_name.begin(), variable_name.end(), '.', '_');

    variable_no_version_name = vname;
    std::replace(variable_no_version_name.begin(), variable_no_version_name.end(), '.', '_');

    target_name_hash = getHashShort();
    hash = getHash();
}

String Package::getTargetName() const
{
    if (target_name.empty())
    {
        auto v = version.toAnyVersion();
        return ppath.toString() + (v == "*" ? "" : ("-" + v));
    }
    return target_name;
}

String Package::getVariableName() const
{
    if (variable_name.empty())
    {
        auto v = version.toAnyVersion();
        auto vname = ppath.toString() + "_" + (v == "*" ? "" : ("_" + v));
        std::replace(vname.begin(), vname.end(), '.', '_');
        return vname;
    }
    return variable_name;
}

Package extractFromString(const String &target)
{
    auto pos = target.rfind('-');
    if (pos == target.npos)
        throw std::runtime_error("Not a package name");

    Package p;
    p.ppath = target.substr(0, pos);
    p.version = target.substr(pos + 1);
    p.createNames();
    return p;
}

Package extractFromStringAny(const String &target)
{
    auto pos = target.rfind('-');

    Package p;
    p.ppath = target.substr(0, pos);
    if (pos != target.npos)
        p.version = target.substr(pos + 1);
    p.createNames();
    return p;
}

void cleanPackages(const String &s, int flags)
{
    // on source flag remove all
    if (flags & CleanTarget::Src)
        flags = CleanTarget::All;

    std::regex r(s);
    PackagesSet pkgs;

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

    cleanPackages(pkgs, flags);

    if (flags & CleanTarget::Src)
    {
        // dependent packages must be rebuilt but with only limited set of the flags
        flags = CleanTarget::Bin |
                CleanTarget::Lib |
                CleanTarget::Obj |
                CleanTarget::Exp ;
    }

    // find dependent packages and remove non installed
    auto dpkgs = getPackagesDatabase().getTransitiveDependentPackages(pkgs);
    for (auto i = dpkgs.begin(); i != dpkgs.end();)
    {
        if (ipkgs.find(*i) == ipkgs.end())
            i = dpkgs.erase(i);
        else
            ++i;
    }

    cleanPackages(dpkgs, flags);
}

void cleanPackage(const Package &pkg, int flags)
{
    static std::unordered_map<Package, int> cleaned_packages;
    static std::shared_mutex m;

    static const auto cache_dir_bin = enumerate_files(directories.storage_dir_bin);
    static const auto cache_dir_exp = enumerate_files(directories.storage_dir_exp);
    static const auto cache_dir_lib = enumerate_files(directories.storage_dir_lib);
#ifdef _WIN32
    static const auto cache_dir_lnk = enumerate_files(directories.storage_dir_lnk);
#endif

    auto &sdb = getServiceDatabase();

    // Clean only installed packages.
    if (sdb.getInstalledPackageId(pkg) == 0)
        return;

    // only clean yet uncleaned flags
    {
        std::shared_lock<std::shared_mutex> lock(m);
        auto i = cleaned_packages.find(pkg);
        if (i != cleaned_packages.end())
            flags &= ~i->second;
        if (flags == 0)
            return;
    }

    // save cleaned packages
    {
        std::unique_lock<std::shared_mutex> lock(m);
        auto &f = cleaned_packages[pkg];
        flags &= ~f;
        f |= flags;

        // double check, we have no upgrade lock
        if (flags == 0)
            return;
    }

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
            boost::system::error_code ec;
            fs::remove_all(p, ec);
        }
    };

    auto rm_recursive = [](const auto &pkg, const auto &files, const auto &ext)
    {
        boost::system::error_code ec;
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
    if (flags & CleanTarget::Exp)
        rm_recursive(pkg, cache_dir_exp, ".cmake");

#ifdef _WIN32
    // solution links
    if (flags & CleanTarget::Lnk)
        rm_recursive(pkg, cache_dir_lnk, ".sln.lnk");
#endif

    // remove packages at the end in case we're removing sources
    if (flags & CleanTarget::Src)
    {
        auto &sdb = getServiceDatabase();
        sdb.removeInstalledPackage(pkg);
    }
}

void cleanPackages(const PackagesSet &pkgs, int flags)
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
