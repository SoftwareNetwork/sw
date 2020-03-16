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

#include "command.h"

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

namespace sw
{

template <class A>
Commands loadCommands(A &);

extern template Commands loadCommands<boost::archive::binary_iarchive>(boost::archive::binary_iarchive &);
extern template Commands loadCommands<boost::archive::text_iarchive>(boost::archive::text_iarchive &);

using SimpleCommands = std::vector<builder::Command *>;

template <class A>
void saveCommands(A &ar, const SimpleCommands &);

extern template void saveCommands<boost::archive::binary_oarchive>(boost::archive::binary_oarchive &, const SimpleCommands &);
extern template void saveCommands<boost::archive::text_oarchive>(boost::archive::text_oarchive &, const SimpleCommands &);

template <class A, class T>
void saveCommands(A &ar, const T &cmds)
{
    SimpleCommands commands;
    commands.reserve(cmds.size());
    for (auto &c : cmds)
        commands.push_back(c.get());
    saveCommands(ar, commands);
}

}
