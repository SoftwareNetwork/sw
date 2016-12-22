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

#include "dependency.h"

#include "config.h"
#include "database.h"
#include "directories.h"
#include "hash.h"
#include "lock.h"

#include <iostream>
#include <regex>

#include <logger.h>
DECLARE_STATIC_LOGGER(logger, "package");

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
    std::ifstream ifile(getStampFilename().string());
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
    target_name   = ppath.toString() + (v == "*" ? "" : ("-" + v));
    variable_name = ppath.toString() + (v == "*" ? "" : ("_" + v));
    std::replace(variable_name.begin(), variable_name.end(), '.', '_');
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
    Package p;
    p.ppath = target.substr(0, target.rfind('-'));
    p.version = target.substr(target.rfind('-') + 1);
    p.createNames();
    return p;
}

void cleanPackages(const String &s, int flags)
{
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

    // find dependent packages and remove non installed
    auto dpkgs = getPackagesDatabase().getTransitiveDependentPackages(pkgs);
    for (auto i = dpkgs.begin(); i != dpkgs.end();)
    {
        if (ipkgs.find(*i) == ipkgs.end())
            i = dpkgs.erase(i);
        else
            ++i;
    }

    auto log = [&flags](const auto &pkgs)
    {
        for (auto &pkg : pkgs)
        {
            if (flags == CleanTarget::All)
                LOG_INFO(logger, "Cleaning   : " + pkg.target_name + "...");
        }
    };

    log(pkgs);
    log(dpkgs);

    cleanPackages(pkgs, flags);

    // dependent packages must be rebuilt but with only limited set of flags
    cleanPackages(dpkgs,
        CleanTarget::Bin |
        CleanTarget::Lib |
        CleanTarget::Obj |
        CleanTarget::Exp);
}

void cleanPackages(const PackagesSet &pkgs, int flags)
{
    auto rm = [](const auto &p)
    {
        if (fs::exists(p))
            fs::remove_all(p);
    };

    auto rm_recursive = [&pkgs](const auto &dir, const auto &ext)
    {
        if (!fs::exists(dir))
            return;
        for (auto &f : boost::make_iterator_range(fs::recursive_directory_iterator(dir), {}))
        {
            if (!fs::is_regular_file(f))
                continue;
            auto fn = f.path().filename().string();
            for (auto &pkg : pkgs)
            {
                if (fn == pkg.target_name + ext)
                    fs::remove(f);
            }
        }
    };

    for (auto &pkg : pkgs)
    {
        if (flags & CleanTarget::Src)
            rm(pkg.getDirSrc());
        if (flags & CleanTarget::Obj)
            rm(pkg.getDirObj());

        if (flags & CleanTarget::Lib)
            remove_files_like(directories.storage_dir_lib, ".*" + pkg.target_name + ".*");
        if (flags & CleanTarget::Bin)
            remove_files_like(directories.storage_dir_bin, ".*" + pkg.target_name + ".*");
    }

    // cmake exports
    if (flags & CleanTarget::Exp)
        rm_recursive(directories.storage_dir_exp, ".cmake");

#ifdef _WIN32
    // solution links
    if (flags & CleanTarget::Lnk)
        rm_recursive(directories.storage_dir_lnk, ".sln.lnk");
#endif

    // remove packages at the end
    auto &sdb = getServiceDatabase();
    for (auto &pkg : pkgs)
        sdb.removeInstalledPackage(pkg);
}
