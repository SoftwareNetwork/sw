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

SUBCOMMAND_DECL(update)
{
    // see https://doc.rust-lang.org/cargo/commands/cargo-update.html
    auto swctx = createSwContext(options);
    auto b = createBuild(*swctx, options.options_update.build_arg_update, options);
    auto bs = b->getSettings();
    if (!options.options_update.packages.empty())
    {
        for (auto &p : options.options_update.packages)
            bs["update_lock_file_packages"][p];
    }
    else
        bs["update_lock_file"] = "true"; // update all
    b->setSettings(bs);
    b->loadInputs();
    b->setTargetsToBuild();
    b->resolvePackages();
}
