/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2020 Egor Pugin
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
#include "command_storage.h"

#include <sw/support/serialization.h>

#include <boost/serialization/access.hpp>
#include <primitives/exceptions.h>

#include <fstream>

#include "command_serialization.h"
#include "command_serialization_boost.h"

namespace sw
{

namespace driver
{
struct Command;
}

template <class A>
Commands loadCommands(A &ar)
{
    Commands commands;
    ar >> commands;
    return commands;
}

template Commands loadCommands<boost::archive::binary_iarchive>(boost::archive::binary_iarchive &);
template Commands loadCommands<boost::archive::text_iarchive>(boost::archive::text_iarchive &);

template <class A>
void saveCommands(A &ar, const SimpleCommands &commands)
{
    ar << commands;
}

template void saveCommands<boost::archive::binary_oarchive>(boost::archive::binary_oarchive &, const SimpleCommands &);
template void saveCommands<boost::archive::text_oarchive>(boost::archive::text_oarchive &, const SimpleCommands &);

Commands loadCommands(const path &p, int type)
{
    return deserialize<Commands>(p, type);
}

void saveCommands(const path &p, const Commands &commands, int type)
{
    serialize(p, commands, type);
}

}
