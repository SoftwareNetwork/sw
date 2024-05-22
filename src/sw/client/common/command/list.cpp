// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>

#include "../commands.h"

#include <sw/manager/package_database.h>
#include <sw/manager/storage.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "list");

std::map<sw::PackagePath, sw::VersionSet> getMatchingPackages(const sw::StorageWithPackagesDatabase &s, const String &arg)
{
    auto &db = s.getPackagesDatabase();

    bool has_version = arg.find('-') != arg.npos;
    sw::UnresolvedPackage u(arg);

    auto ppaths = db.getMatchingPackages(u.getPath().toString());
    if (ppaths.empty())
        return {};

    std::map<sw::PackagePath, sw::VersionSet> r;
    for (auto &ppath : ppaths)
    {
        auto v1 = db.getVersionsForPackage(ppath);
        for (auto &v : v1)
        {
            if (!has_version || u.getRange().hasVersion(v))
                r[ppath].insert(v);
        }
    }
    return r;
}

SUBCOMMAND_DECL(list)
{
    const sw::StorageWithPackagesDatabase *s;
    auto rs = getContext().getRemoteStorages();

    std::map<sw::PackagePath, sw::VersionSet> r;
    if (getOptions().options_list.installed)
    {
        s = &getContext().getLocalStorage();
        r = getMatchingPackages(*s, getOptions().options_list.list_arg);
    }
    else
    {
        if (rs.empty())
            throw SW_RUNTIME_ERROR("No remote storages found");

        // merge from
        for (auto &s : rs)
        {
            auto m = getMatchingPackages(static_cast<sw::StorageWithPackagesDatabase &>(*s), getOptions().options_list.list_arg);
            for (auto &[p, v] : m)
                r[p].merge(v);
        }

        // overridden storage
        {
            auto m = getMatchingPackages(getContext().getLocalStorage().getOverriddenPackagesStorage(), getOptions().options_list.list_arg);
            for (auto &[p, v] : m)
                r[p].merge(v);
        }
        ;
    }

    if (r.empty())
    {
        LOG_INFO(logger, "nothing found");
        return;
    }

    for (auto &[ppath, versions] : r)
    {
        String out = ppath.toString();
        out += " (";
        for (auto vi = versions.rbegin(); vi != versions.rend(); vi++)
            out += vi->toString() + ", ";
        out.resize(out.size() - 2);
        out += ")";
        LOG_INFO(logger, out);
    }
}
