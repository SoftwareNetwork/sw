// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "../command.h"
#include "../options_cl.h"

namespace sw
{

template <class T>
static void getCommandLineOptions(driver::Command *c, const CommandLineOptions<T> &t, const String prefix = "", bool end_options = false)
{
    for (auto &o : t)
    {
        if (o.manual_handling)
            continue;
        if (end_options != o.place_at_the_end)
            continue;
        auto cmd = o.getCommandLine(c);
        for (auto &c2 : cmd)
        {
            if (!prefix.empty())
                c->arguments.push_back(prefix);
            c->arguments.push_back(c2);
        }
    }
}

}
