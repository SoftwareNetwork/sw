/*
 * SW - Build System and Package Manager
 * Copyright (C) 2020 Egor Pugin
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

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "command.path");

SUBCOMMAND_DECL(path)
{
    auto m = getContext(false).install(sw::UnresolvedPackages{getOptions().options_path.path_arg});
    auto i = m.find(getOptions().options_path.path_arg);
    if (i == m.end())
        return;
    auto &p = i->second;

    if (getOptions().options_path.type == "sdir")
    {
        LOG_INFO(logger, normalize_path(p.getDirSrc2()));
    }
}
