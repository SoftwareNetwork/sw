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
#include "hash.h"
#include "lock.h"
#include "directories.h"

#include <iostream>
#include <regex>

const String cppan_index_file = "index.txt";
const String cppan_package_dependencies_file = "dependencies.db.txt";

path Package::getDirSrc() const
{
    return directories.storage_dir_src / getHashPath();
}

path Package::getDirObj() const
{
    return directories.storage_dir_obj / getHashPath();
}

path Package::getStampFilename() const
{
    auto b = directories.storage_dir_etc / STAMPS_DIR / "packages" / getHashPath();
    auto f = b.filename();
    b = b.parent_path();
    b /= get_stamp_filename(f.string());
    return b;
}

String Package::getHash() const
{
    static const auto delim = "/";
    if (hash.empty())
        return sha256(ppath.toString() + delim + version.toString());
    return hash;
}

String Package::getFilesystemHash() const
{
    return getHash().substr(0, 8);
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
    variable_name = ppath.toString() + "_" + (v == "*" ? "" : ("_" + v));
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
    p.ppath = target.substr(0, target.find('-'));
    p.version = target.substr(target.find('-') + 1);
    return p;
}

PackageIndex readPackagesIndex(const path &dir)
{
    auto fn = dir / cppan_index_file;
    ScopedShareableFileLock lock(fn);

    PackageIndex pkgs;
    std::ifstream ifile(fn.string());
    if (!ifile)
        return pkgs;

    String target_name;
    path p;
    while (ifile >> p >> target_name)
        pkgs[target_name] = p;

    return pkgs;
}

void writePackagesIndex(const path &dir, const PackageIndex &idx)
{
    auto fn = dir / cppan_index_file;
    ScopedFileLock lock(fn);

    std::ofstream ofile(fn.string());
    if (!ofile)
        return;

    for (auto &pkg : idx)
        ofile << normalize_path(pkg.second) << "\t\t" << pkg.first << "\n";
}

void cleanPackages(const String &s, int flags)
{
    std::regex r(s);

    auto remove = [&s, &r](const auto &dir)
    {
        auto pkgs = readPackagesIndex(dir);
        std::vector<String> rms;
        for (auto &pkg : pkgs)
        {
            if (!std::regex_match(pkg.first, r))
                continue;
            if (fs::exists(pkg.second))
                fs::remove_all(pkg.second);
            rms.push_back(pkg.first);
        }
        for (auto &rm : rms)
            pkgs.erase(rm);
        writePackagesIndex(dir, pkgs);
    };
    if (flags & CleanTarget::Src)
        remove(directories.storage_dir_src);
    if (flags & CleanTarget::Obj)
        remove(directories.storage_dir_obj);
    if (flags & CleanTarget::Lib)
        remove_files_like(directories.storage_dir_lib, s);
    if (flags & CleanTarget::Bin)
        remove_files_like(directories.storage_dir_bin, s);
}

PackageDependenciesIndex readPackageDependenciesIndex(const path &dir)
{
    auto fn = dir / cppan_package_dependencies_file;
    ScopedShareableFileLock lock(fn);

    PackageDependenciesIndex pkgs;
    std::ifstream ifile(fn.string());
    if (!ifile)
        return pkgs;

    String target_name, hash;
    while (ifile >> hash >> target_name)
        pkgs[target_name] = hash;

    return pkgs;
}

void writePackageDependenciesIndex(const path &dir, const PackageDependenciesIndex &idx)
{
    auto fn = dir / cppan_package_dependencies_file;
    ScopedFileLock lock(fn);

    std::ofstream ofile(fn.string());
    if (!ofile)
        return;

    for (auto &pkg : idx)
        if (!pkg.second.empty())
            ofile << pkg.second << "\t" << pkg.first << "\n";
}
