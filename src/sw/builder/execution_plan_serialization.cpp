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

// headers for serialization
#include <boost/serialization/access.hpp>
#include <boost/serialization/base_object.hpp>

#include "command_storage.h"
//

#include "execution_plan.h"

#include "sw_context.h"

#include <sw/support/serialization.h>

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
