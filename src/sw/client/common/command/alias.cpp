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

/*#include <sw/builder/program.h>
#include <sw/core/input.h>
#include <sw/manager/storage.h>
#include <sw/support/filesystem.h>*/

#include <nlohmann/json.hpp>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "alias");

static auto get_aliases_fn()
{
    return sw::get_root_directory() / "aliases.json";
}

Strings SwClientContext::getAliasArguments(const String &name)
{
    auto aliases_db = get_aliases_fn();
    if (!fs::exists(aliases_db))
        return {};
    auto j = nlohmann::json::parse(read_file(aliases_db));
    if (j.contains(name))
        return j[name];
    return {};
}

SUBCOMMAND_DECL(alias)
{
    auto &name = getOptions().options_alias.name;
    if (name.empty())
        throw SW_RUNTIME_ERROR("Empty name");
    auto cmds = listCommands();
    if (cmds.find(name) != cmds.end())
        throw SW_RUNTIME_ERROR("Cannot create alias to existing command.");

    if (getOptions().options_alias.delete_alias)
    {
        auto aliases_db = get_aliases_fn();
        if (!fs::exists(aliases_db))
            return;
        auto j = nlohmann::json::parse(read_file(aliases_db));
        j.erase(name);
        write_file(aliases_db, j.dump(4));
        return;
    }

    auto &args = getOptions().options_alias.arguments;
    if (args.empty())
        throw SW_RUNTIME_ERROR("Empty arguments");

    auto aliases_db = get_aliases_fn();
    nlohmann::json j;
    if (fs::exists(aliases_db))
        j = nlohmann::json::parse(read_file(aliases_db));
    j[name].clear();
    for (auto &a : args)
        j[name].push_back(a);
    write_file(aliases_db, j.dump(4));
}
