// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>

#include "../commands.h"

SUBCOMMAND_DECL(update)
{
    // see https://doc.rust-lang.org/cargo/commands/cargo-update.html
    auto b = createBuild(getOptions().options_update.build_arg_update);
    auto bs = b->getSettings();
    if (!getOptions().options_update.packages.empty())
    {
        for (auto &p : getOptions().options_update.packages)
            bs["update_lock_file_packages"][p];
    }
    else
        bs["update_lock_file"] = "true"; // update all
    b->setSettings(bs);
    b->loadInputs();
    SW_UNIMPLEMENTED;
    //b->setTargetsToBuild();
    b->resolvePackages();
}
