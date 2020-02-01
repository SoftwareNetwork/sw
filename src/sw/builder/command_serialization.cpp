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

// change when you change the header above
static const int serialization_version = 3;

namespace sw
{

namespace driver
{
struct Command;
}

enum SerializationType
{
    BoostSerializationBinaryArchive,
    BoostSerializationTextArchive,
};

template <class A>
Commands loadCommands(A &ar)
{
    int version;
    ar >> version;
    if (version != serialization_version)
    {
        throw SW_RUNTIME_ERROR("Incorrect archive version (" + std::to_string(version) + "), expected (" +
            std::to_string(serialization_version) + "), run configure command again");
    }
    Commands commands;
    ar >> commands;
    return commands;
}

template Commands loadCommands<boost::archive::binary_iarchive>(boost::archive::binary_iarchive &);
template Commands loadCommands<boost::archive::text_iarchive>(boost::archive::text_iarchive &);

template <class A>
void saveCommands(A &ar, const SimpleCommands &commands)
{
    ar << serialization_version;
    ar << commands;
}

template void saveCommands<boost::archive::binary_oarchive>(boost::archive::binary_oarchive &, const SimpleCommands &);
template void saveCommands<boost::archive::text_oarchive>(boost::archive::text_oarchive &, const SimpleCommands &);

Commands loadCommands(const path &p, int type)
{
    if (type == SerializationType::BoostSerializationBinaryArchive)
    {
        std::ifstream ifs(p, std::ios_base::in | std::ios_base::binary);
        if (!ifs)
            throw SW_RUNTIME_ERROR("Cannot read file: " + normalize_path(p));
        boost::archive::binary_iarchive ia(ifs);
        return loadCommands(ia);
    }
    else if (type == SerializationType::BoostSerializationTextArchive)
    {
        std::ifstream ifs(p);
        if (!ifs)
            throw SW_RUNTIME_ERROR("Cannot read file: " + normalize_path(p));
        boost::archive::text_iarchive ia(ifs);
        return loadCommands(ia);
    }
    throw SW_RUNTIME_ERROR("Bad type");
}

void saveCommands(const path &p, const Commands &commands, int type)
{
    if (type == SerializationType::BoostSerializationBinaryArchive)
    {
        std::ofstream ofs(p, std::ios_base::out | std::ios_base::binary);
        if (!ofs)
            throw SW_RUNTIME_ERROR("Cannot write file: " + normalize_path(p));
        boost::archive::binary_oarchive oa(ofs);
        return save(oa, commands);
    }
    else if (type == SerializationType::BoostSerializationTextArchive)
    {
        std::ofstream ofs(p);
        if (!ofs)
            throw SW_RUNTIME_ERROR("Cannot write file: " + normalize_path(p));
        boost::archive::text_oarchive oa(ofs);
        return save(oa, commands);
    }
    throw SW_RUNTIME_ERROR("Bad type");
}

}
