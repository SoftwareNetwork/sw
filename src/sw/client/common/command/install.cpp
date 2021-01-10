// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>

#include "../commands.h"

#include <sw/manager/package.h>

SUBCOMMAND_DECL(install)
{
    std::unordered_set<sw::UnresolvedPackageName> pkgs;
    getOptions().options_install.install_args.push_back(getOptions().options_install.install_arg);
    for (auto &p : getOptions().options_install.install_args)
        pkgs.insert(sw::extractFromString(p));
    SW_UNIMPLEMENTED;
    //auto m = getContext().install(pkgs);
    //for (auto &[p1, d] : m)
    {
        //for (auto &p2 : install_args)
        //if (p1 == p2)
        //d.installed = true;
    }
}
