// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>

#include "../commands.h"

#include <sw/manager/remote.h>
#include <sw/manager/settings.h>

static sw::Remote &find_remote_raw(sw::Settings &s, const String &name)
{
    sw::Remote *current_remote = nullptr;
    for (auto &r : s.getRemotes(false))
    {
        if (r->name == name)
        {
            current_remote = r.get();
            break;
        }
    }
    if (!current_remote)
        throw SW_RUNTIME_ERROR("Remote not found: " + name);
    return *current_remote;
}

sw::Remote &find_remote(sw::Settings &s, const String &name)
{
    sw::Remote &current_remote = find_remote_raw(s, name);
    if (current_remote.isDisabled())
        throw SW_RUNTIME_ERROR("Remote is disabled: " + name);
    return current_remote;
}

SUBCOMMAND_DECL(remote)
{
    // subcommands: add, alter, rename, remove, enable, disable

    // sw remote add origin url:port
    // sw remote remove origin
    // sw remote rename origin origin2
    // sw remote alter origin add token TOKEN
    // sw remote enable origin
    // sw remote disable origin

    if (getOptions().options_remote.remote_subcommand == "alter" || getOptions().options_remote.remote_subcommand == "change")
    {
        int i = 0;
        if (getOptions().options_remote.remote_rest.size() > i + 1)
        {
            auto token = getOptions().options_remote.remote_rest[i];
            auto &us = sw::Settings::get_user_settings();
            auto &r = find_remote(us, getOptions().options_remote.remote_rest[i]);

            i++;
            if (getOptions().options_remote.remote_rest.size() > i + 1)
            {
                if (getOptions().options_remote.remote_rest[i] == "add")
                {
                    i++;
                    if (getOptions().options_remote.remote_rest.size() > i + 1)
                    {
                        if (getOptions().options_remote.remote_rest[i] == "token")
                        {
                            i++;
                            if (getOptions().options_remote.remote_rest.size() >= i + 2) // publisher + token
                            {
                                sw::Remote::Publisher p;
                                p.name = getOptions().options_remote.remote_rest[i];
                                p.token = getOptions().options_remote.remote_rest[i+1];
                                r.publishers[p.name] = p;
                                us.save(sw::support::get_config_filename());
                            }
                            else
                                throw SW_RUNTIME_ERROR("missing publisher or token");
                        }
                        else
                            throw SW_RUNTIME_ERROR("unknown add object: " + getOptions().options_remote.remote_rest[i]);
                    }
                    else
                        throw SW_RUNTIME_ERROR("missing add object");
                }
                else
                    throw SW_RUNTIME_ERROR("unknown alter command: " + getOptions().options_remote.remote_rest[i]);
            }
            else
                throw SW_RUNTIME_ERROR("missing alter command");
        }
        else
            throw SW_RUNTIME_ERROR("missing remote name");
        return;
    }

    if (getOptions().options_remote.remote_subcommand == "enable")
    {
        int i = 0;
        if (getOptions().options_remote.remote_rest.size() > i)
        {
            auto name = getOptions().options_remote.remote_rest[i];
            auto &us = sw::Settings::get_user_settings();
            auto &r = find_remote_raw(us, name);
            r.disabled = false;
            us.save(sw::support::get_config_filename());
        }
        else
            throw SW_RUNTIME_ERROR("missing remote name");
        return;
    }

    if (getOptions().options_remote.remote_subcommand == "disable")
    {
        int i = 0;
        if (getOptions().options_remote.remote_rest.size() > i)
        {
            auto name = getOptions().options_remote.remote_rest[i];
            auto &us = sw::Settings::get_user_settings();
            auto &r = find_remote_raw(us, name);
            r.disabled = true;
            us.save(sw::support::get_config_filename());
        }
        else
            throw SW_RUNTIME_ERROR("missing remote name");
        return;
    }

    throw SW_RUNTIME_ERROR("Unknown subcommand: " + getOptions().options_remote.remote_subcommand);
}
