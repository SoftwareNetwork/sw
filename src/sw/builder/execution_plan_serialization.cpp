// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "execution_plan.h"

#include "sw_context.h"

#include <sw/support/serialization.h>
#include "command_serialization.h"

namespace sw
{

std::tuple<Commands, ExecutionPlan>
ExecutionPlan::load(const path &p, const SwBuilderContext &swctx, int type)
{
    Commands commands;

    auto load = [&commands](auto &ar)
    {
        path cp;
        ar >> cp;
        fs::current_path(cp);
        commands = loadCommands(ar);
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
    {
        c->setContext(swctx);
        c->command_storage = &swctx.getCommandStorage(c->command_storage_root);
    }
    return { commands, create(commands) };
}

void ExecutionPlan::save(const path &p, int type) const
{
    fs::create_directories(p.parent_path());

    auto save = [this](auto &ar)
    {
        ar << fs::current_path();
        //               v be careful... v
        saveCommands(ar, (SimpleCommands&)commands);
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
