// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
