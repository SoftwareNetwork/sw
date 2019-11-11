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

#include <sw/manager/package.h>

DEFINE_SUBCOMMAND(install, "Add package to lock.");
DEFINE_SUBCOMMAND_ALIAS(install, i)

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
