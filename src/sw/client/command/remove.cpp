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
