// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "commands.h"

#include <sw/manager/database.h>
#include <sw/manager/storage.h>

static ::cl::opt<String> list_arg(::cl::Positional, ::cl::desc("Package regex to list"), ::cl::init("."), ::cl::sub(subcommand_list));

SUBCOMMAND_DECL(list)
{
    auto swctx = createSwContext();
    auto rs = swctx->getRemoteStorages();
    if (rs.empty())
        throw SW_RUNTIME_ERROR("No remote storages found");

    static_cast<sw::StorageWithPackagesDatabase&>(*rs.front()).getPackagesDatabase().listPackages(list_arg);
}
