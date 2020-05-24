// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

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

    if (getOptions().options_alias.print_alias)
    {
        auto aliases_db = get_aliases_fn();
        if (!fs::exists(aliases_db))
            return;
        auto j = nlohmann::json::parse(read_file(aliases_db));
        if (!j.contains(name))
            throw SW_RUNTIME_ERROR("No such alias");
        String s;
        for (auto &a : j[name])
            s += a.get<String>() + " ";
        LOG_INFO(logger, s);
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
