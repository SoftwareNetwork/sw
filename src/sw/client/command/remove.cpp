// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "commands.h"

static ::cl::list<String> remove_arg(::cl::Positional, ::cl::desc("package to remove"), ::cl::sub(subcommand_remove));

SUBCOMMAND_DECL(remove)
{
    auto swctx = createSwContext();
    for (auto &a : remove_arg)
    {
        sw::LocalPackage p(swctx->getLocalStorage(), a);
        //sdb.removeInstalledPackage(p); // TODO: remove from db
        fs::remove_all(p.getDir());
    }
}
