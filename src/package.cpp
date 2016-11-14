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
    p.ppath = target.substr(0, target.rfind('-'));
    p.version = target.substr(target.rfind('-') + 1);
    return p;
}

void cleanPackages(const String &s, int flags)
{
    std::regex r(s);

    auto &sdb = getServiceDatabase();

    auto remove = [&sdb, &s, &r](auto f)
    {
        auto pkgs = sdb.getInstalledPackages();
        for (auto &pkg : pkgs)
        {
            if (!std::regex_match(pkg.target_name, r))
                continue;
            auto p = (pkg.*f)();
            if (fs::exists(p))
                fs::remove_all(p);
            sdb.removeInstalledPackage(pkg);
        }
    };
    if (flags & CleanTarget::Src)
        remove(&Package::getDirSrc);
    if (flags & CleanTarget::Obj)
        remove(&Package::getDirObj);
    if (flags & CleanTarget::Lib)
        remove_files_like(directories.storage_dir_lib, s);
    if (flags & CleanTarget::Bin)
        remove_files_like(directories.storage_dir_bin, s);
}
