/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2018 Egor Pugin
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

#pragma once

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
