// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "commands.h"

#include <sw/manager/package.h>

static ::cl::opt<String> install_arg(::cl::Positional, ::cl::desc("Packages to add"), ::cl::sub(subcommand_install));
static ::cl::list<String> install_args(::cl::ConsumeAfter, ::cl::desc("Packages to add"), ::cl::sub(subcommand_install));

SUBCOMMAND_DECL(install)
{
    auto swctx = createSwContext();
    sw::UnresolvedPackages pkgs;
    install_args.push_back(install_arg);
    for (auto &p : install_args)
        pkgs.insert(sw::extractFromString(p));
    auto m = swctx->install(pkgs);
    for (auto &[p1, d] : m)
    {
        //for (auto &p2 : install_args)
        //if (p1 == p2)
        //d.installed = true;
    }
}
