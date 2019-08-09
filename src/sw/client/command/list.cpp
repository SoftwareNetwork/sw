/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2019 Egor Pugin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "commands.h"

#include <sw/manager/database.h>
#include <sw/manager/storage.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "list");

DEFINE_SUBCOMMAND(list, "List packages in database.");

static ::cl::opt<String> list_arg(::cl::Positional, ::cl::desc("Package regex to list"), ::cl::init("."), ::cl::sub(subcommand_list));

std::map<sw::PackagePath, sw::VersionSet> getMatchingPackages(const sw::StorageWithPackagesDatabase &s, const sw::UnresolvedPackage &u)
{
    auto &db = s.getPackagesDatabase();

    auto ppaths = db.getMatchingPackages(u.getPath().toString());
    if (ppaths.empty())
        return {};

    std::map<sw::PackagePath, sw::VersionSet> r;
    for (auto &ppath : ppaths)
    {
        auto v1 = db.getVersionsForPackage(ppath);
        for (auto &v : v1)
        {
            if (u.getRange().hasVersion(v))
                r[ppath].insert(v);
        }
    }
    return r;
}

sw::PackageIdSet getMatchingPackagesSet(const sw::StorageWithPackagesDatabase &s, const sw::UnresolvedPackage &u)
{
    sw::PackageIdSet p;
    for (auto &[ppath, versions] : getMatchingPackages(s, u))
    {
        for (auto &v : versions)
            p.emplace(ppath, v);
    }
    return p;
}

SUBCOMMAND_DECL(list)
{
    auto swctx = createSwContext();
    auto rs = swctx->getRemoteStorages();
    if (rs.empty())
        throw SW_RUNTIME_ERROR("No remote storages found");

    auto &s = static_cast<sw::StorageWithPackagesDatabase &>(*rs.front());

    auto r = getMatchingPackages(s, list_arg);
    if (r.empty())
    {
        LOG_INFO(logger, "nothing found");
        return;
    }

    for (auto &[ppath, versions] : r)
    {
        String out = ppath.toString();
        out += " (";
        for (auto &v : versions)
            out += v.toString() + ", ";
        out.resize(out.size() - 2);
        out += ")";
        LOG_INFO(logger, out);
    }
}
