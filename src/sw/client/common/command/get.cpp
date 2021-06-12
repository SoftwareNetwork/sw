// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#include "../commands.h"

#include <sw/manager/storage.h>
#include <sw/manager/sw_context.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "get");

SUBCOMMAND_DECL(get)
{
    auto &args = getOptions().options_get.args;
    if (args.empty())
        throw SW_RUNTIME_ERROR("Empty args");
    if (args.at(0) == "storage-dir")
    {
        LOG_INFO(logger, to_printable_string(getContext(false).getLocalStorage().storage_dir));
        return;
    }
    throw SW_RUNTIME_ERROR("Unknown command");
}
