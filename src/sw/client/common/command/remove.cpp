// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>

#include "../commands.h"

#include <sw/manager/storage.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "remove");

static sw::PackageIdSet getMatchingPackagesSet(const sw::StorageWithPackagesDatabase &s, const String &unresolved_pkg)
{
    sw::PackageIdSet p;
    for (auto &[ppath, versions] : getMatchingPackages(s, unresolved_pkg))
    {
        for (auto &v : versions)
            p.emplace(ppath, v);
    }
    return p;
}

SUBCOMMAND_DECL(remove)
{
    for (auto &a : getOptions().options_remove.remove_arg)
    {
        for (auto &p : getMatchingPackagesSet(getContext().getLocalStorage(), a))
        {
            LOG_INFO(logger, "Removing " << p.toString());
            getContext().getLocalStorage().remove(sw::LocalPackage(getContext().getLocalStorage(), p));
        }
    }
}
