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
#include <boost/serialization/export.hpp>

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

#include "command_storage.h"
//

#include "execution_plan.h"

#include "sw_context.h"

#include <sw/support/serialization.h>

#define SERIALIZATION_TYPE sw::builder::Command
SERIALIZATION_BEGIN_UNIFIED
    ar & boost::serialization::base_object<::primitives::Command>(v);

    ar & v.name;

    size_t flag = (size_t)v.command_storage;
    ar & flag;
    if (flag > 1)
    {
        if (v.command_storage)
            ar & v.command_storage->root;
        else
        {
            path p;
            ar & p;
            v.command_storage_root = p;
        }
    }
    v.command_storage = (::sw::CommandStorage*)flag;

    ar & v.deps_processor;
    ar & v.deps_module;
    ar & v.deps_function;
    ar & v.deps_file;
    ar & v.msvc_prefix;

    ar & v.first_response_file_argument;
    ar & v.always;
    ar & v.remove_outputs_before_execution;
    ar & v.strict_order;
    ar & v.output_dirs;

    ar & v.inputs;
    ar & v.outputs;

    //ar & dependent_commands;
    //ar & dependencies;
SERIALIZATION_UNIFIED_END

#define SERIALIZATION_TYPE primitives::Command::Arguments
SERIALIZATION_BEGIN_SPLIT
    size_t sz;
    ar & sz;
    while (sz--)
    {
        String s;
        ar & s;
        v.push_back(std::make_unique<primitives::command::SimpleArgument>(s));
    }
SERIALIZATION_SPLIT_CONTINUE
    ar & v.size();
    for (auto &a : v)
        ar & a->toString();
SERIALIZATION_SPLIT_END

#define SERIALIZATION_TYPE sw::ExecutionPlan::VecT
SERIALIZATION_BEGIN_SPLIT
    SW_UNIMPLEMENTED;
SERIALIZATION_SPLIT_CONTINUE
    ar & v.size();
    for (auto &a : v)
        ar & (sw::builder::Command&)*a;
SERIALIZATION_SPLIT_END

#define SERIALIZATION_TYPE sw::Commands
SERIALIZATION_BEGIN_SPLIT
    size_t sz;
    ar & sz;
    while (sz--)
    {
        auto c = std::make_shared<sw::builder::Command>();
        ar & *c;
        v.insert(c);
    }
SERIALIZATION_SPLIT_CONTINUE
    SW_UNIMPLEMENTED;
SERIALIZATION_SPLIT_END

namespace sw
{

Commands ExecutionPlan::load(const path &p, const SwBuilderContext &swctx, int type)
{
    Commands commands;

    auto load = [&commands](auto &ar)
    {
        path cp;
        ar >> cp;
        fs::current_path(cp);
        ar >> commands;
    };

    type = 1;
    if (type == 0)
    {
        std::ifstream ifs(p, std::ios_base::in | std::ios_base::binary);
        if (!ifs)
            throw SW_RUNTIME_ERROR("Cannot read file: " + to_string(p));
        boost::archive::binary_iarchive ar(ifs);
        load(ar);
    }
    else if (type == 1)
    {
        std::ifstream ifs(p);
        if (!ifs)
            throw SW_RUNTIME_ERROR("Cannot read file: " + to_string(p));
        boost::archive::text_iarchive ar(ifs);
        load(ar);
    }

    // some setup
    for (auto &c : commands)
    {
        c->setContext(swctx);
        c->command_storage = &swctx.getCommandStorage(c->command_storage_root);
    }
    return commands;
}

void ExecutionPlan::save(const path &p, int type) const
{
    fs::create_directories(p.parent_path());

    auto save = [this](auto &ar)
    {
        ar << fs::current_path();
        ar << commands;
    };

    type = 1;
    if (type == 0)
    {
        std::ofstream ofs(p, std::ios_base::out | std::ios_base::binary);
        if (!ofs)
            throw SW_RUNTIME_ERROR("Cannot write file: " + to_string(p));
        boost::archive::binary_oarchive ar(ofs);
        save(ar);
    }
    else if (type == 1)
    {
        std::ofstream ofs(p);
        if (!ofs)
            throw SW_RUNTIME_ERROR("Cannot write file: " + to_string(p));
        boost::archive::text_oarchive ar(ofs);
        save(ar);
    }
}

}
