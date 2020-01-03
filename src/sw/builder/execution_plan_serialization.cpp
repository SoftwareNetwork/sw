// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "execution_plan.h"

#include <sw/support/serialization.h>

#include <boost/serialization/access.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <primitives/exceptions.h>

#include <fstream>

#include "execution_plan_serialization_boost.h"

static const int serialization_version = 2;

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

std::tuple<std::unordered_set<std::shared_ptr<builder::Command>>, ExecutionPlan>
ExecutionPlan::load(const path &p, const SwBuilderContext &swctx, int type)
{
    std::unordered_set<std::shared_ptr<builder::Command>> commands;

    auto load = [&commands](auto &ar)
    {
        int version;
        ar >> version;
        if (version != serialization_version)
        {
            throw SW_RUNTIME_ERROR("Incorrect archive version (" + std::to_string(version) + "), expected (" +
                std::to_string(serialization_version) + "), run configure command again");
        }
        path cp;
        ar >> cp;
        fs::current_path(cp);
        ar >> commands;
    };

    if (type == 0)
    {
        std::ifstream ifs(p, std::ios_base::in | std::ios_base::binary);
        if (!ifs)
            throw SW_RUNTIME_ERROR("Cannot read file: " + normalize_path(p));
        boost::archive::binary_iarchive ia(ifs);
        load(ia);
    }
    else if (type == 1)
    {
        std::ifstream ifs(p);
        if (!ifs)
            throw SW_RUNTIME_ERROR("Cannot read file: " + normalize_path(p));
        boost::archive::text_iarchive ia(ifs);
        load(ia);
    }

    // some setup
    for (auto &c : commands)
        c->setContext(swctx);
    return { commands, create(commands) };
}

void ExecutionPlan::save(const path &p, int type) const
{
    fs::create_directories(p.parent_path());

    auto save = [this](auto &ar)
    {
        ar << serialization_version;
        ar << fs::current_path();
        ar << commands;
    };

    if (type == 0)
    {
        std::ofstream ofs(p, std::ios_base::out | std::ios_base::binary);
        if (!ofs)
            throw SW_RUNTIME_ERROR("Cannot write file: " + normalize_path(p));
        boost::archive::binary_oarchive oa(ofs);
        save(oa);
    }
    else if (type == 1)
    {
        std::ofstream ofs(p);
        if (!ofs)
            throw SW_RUNTIME_ERROR("Cannot write file: " + normalize_path(p));
        boost::archive::text_oarchive oa(ofs);
        save(oa);
    }
}

}
