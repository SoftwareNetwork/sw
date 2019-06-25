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

#include <sw/manager/remote.h>
#include <sw/manager/settings.h>

static ::cl::opt<String> remote_subcommand(::cl::Positional, ::cl::desc("remote subcomand"), ::cl::sub(subcommand_remote), ::cl::Required);
static ::cl::list<String> remote_rest(::cl::desc("other remote args"), ::cl::sub(subcommand_remote), ::cl::ConsumeAfter);

sw::Remote *find_remote(sw::Settings &s, const String &name)
{
    sw::Remote *current_remote = nullptr;
    for (auto &r : s.remotes)
    {
        if (r.name == name)
        {
            current_remote = &r;
            break;
        }
    }
    if (!current_remote)
        throw SW_RUNTIME_ERROR("Remote not found: " + name);
    return current_remote;
}

SUBCOMMAND_DECL(remote)
{
    // subcommands: add, alter, rename, remove

    // sw remote add origin url:port
    // sw remote remove origin
    // sw remote rename origin origin2
    // sw remote alter origin add token TOKEN

    if (remote_subcommand == "alter" || remote_subcommand == "change")
    {
        int i = 0;
        if (remote_rest.size() > i + 1)
        {
            auto token = remote_rest[i];
            auto &us = sw::Settings::get_user_settings();
            auto r = find_remote(us, remote_rest[i]);

            i++;
            if (remote_rest.size() > i + 1)
            {
                if (remote_rest[i] == "add")
                {
                    i++;
                    if (remote_rest.size() > i + 1)
                    {
                        if (remote_rest[i] == "token")
                        {
                            i++;
                            if (remote_rest.size() >= i + 2) // publisher + token
                            {
                                sw::Remote::Publisher p;
                                p.name = remote_rest[i];
                                p.token = remote_rest[i+1];
                                r->publishers[p.name] = p;
                                us.save(sw::get_config_filename());
                            }
                            else
                                throw SW_RUNTIME_ERROR("missing publisher or token");
                        }
                        else
                            throw SW_RUNTIME_ERROR("unknown add object: " + remote_rest[i]);
                    }
                    else
                        throw SW_RUNTIME_ERROR("missing add object");
                }
                else
                    throw SW_RUNTIME_ERROR("unknown alter command: " + remote_rest[i]);
            }
            else
                throw SW_RUNTIME_ERROR("missing alter command");
        }
        else
            throw SW_RUNTIME_ERROR("missing remote name");
        return;
    }
}
