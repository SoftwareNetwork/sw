// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#include "../commands.h"

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "command.path");

SUBCOMMAND_DECL(path)
{
    SW_UNIMPLEMENTED;
    /*auto m = getContext(false).install(sw::UnresolvedPackages{getOptions().options_path.path_arg});
    auto i = m.find(getOptions().options_path.path_arg);
    if (i == m.end())
        return;
    auto &p = i->second;

    if (getOptions().options_path.type == "sdir")
    {
        LOG_INFO(logger, to_string(normalize_path(p.getDirSrc2())));
    }*/
}
