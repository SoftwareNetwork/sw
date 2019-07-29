// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "execution_plan.h"

#include <primitives/exceptions.h>

#include <fstream>

#include "execution_plan_serialization_boost.h"

namespace sw
{

enum SerializationType
{
    BoostSerializationBinaryArchive,
    BoostSerializationTextArchive,
};

ExecutionPlan ExecutionPlan::load(const path &p, const SwBuilderContext &swctx, int type)
{
    // TODO: memory leak
    auto commands = new std::unordered_set<std::shared_ptr<builder::Command>>;
    if (type == 0)
    {
        std::ifstream ifs(p, std::ios_base::in | std::ios_base::binary);
        boost::archive::binary_iarchive ia(ifs);
        ia >> commands;
    }
    else if (type == 1)
    {
        std::ifstream ifs(p);
        boost::archive::text_iarchive ia(ifs);
        ia >> commands;
    }
    for (auto &c : *commands)
        c->setContext(swctx);
    return createExecutionPlan(*commands);
}

void ExecutionPlan::save(const path &p, int type) const
{
    fs::create_directories(p.parent_path());

    if (type == 0)
    {
        std::ofstream ofs(p, std::ios_base::out | std::ios_base::binary);
        boost::archive::binary_oarchive oa(ofs);
        oa << commands;
    }
    else if (type == 1)
    {
        std::ofstream ofs(p);
        boost::archive::text_oarchive oa(ofs);
        oa << commands;
    }
}

}
