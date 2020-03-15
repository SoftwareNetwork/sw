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

#include "../commands.h"

#include <sw/manager/package.h>

SUBCOMMAND_DECL(install)
{
    sw::UnresolvedPackages pkgs;
    getOptions().options_install.install_args.push_back(getOptions().options_install.install_arg);
    for (auto &p : getOptions().options_install.install_args)
        pkgs.insert(sw::extractFromString(p));
    auto m = getContext().install(pkgs);
    for (auto &[p1, d] : m)
    {
        //for (auto &p2 : install_args)
        //if (p1 == p2)
        //d.installed = true;
    }
}
