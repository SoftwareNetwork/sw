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

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4005) // warning C4005: 'XXX': macro redefinition
#endif

////////////////////////////////////////////////////////////////////////////////

#define SERIALIZATION_TYPE ::sw::builder::Command::Argument
SERIALIZATION_BEGIN_UNIFIED
    ar & v.toString();
SERIALIZATION_UNIFIED_END

////////////////////////////////////////

#define SERIALIZATION_TYPE ::primitives::Command::Stream
SERIALIZATION_BEGIN_UNIFIED
    ar & v.text;
    ar & v.file;
    ar & v.append;
SERIALIZATION_UNIFIED_END

////////////////////////////////////////

#define SERIALIZATION_TYPE ::primitives::Command
SERIALIZATION_BEGIN_SPLIT
    ar & v.working_directory;
    ar & v.environment;

    ar & v.in;
    ar & v.out;
    ar & v.err;

    /*if (v.next)
        throw SW_RUNTIME_ERROR("Some error");
    ar & v.prev;*/

    size_t sz;
    ar >> sz;
    String s;
    ar >> s;
    sz--;
    v.setProgram(s);
    while (sz--)
    {
        ar >> s;
        v.push_back(s);
    }
SERIALIZATION_SPLIT_CONTINUE
    ar & v.working_directory;
    ar & v.environment;

    ar & v.in;
    ar & v.out;
    ar & v.err;

    //ar & v.prev;

    ar << v.arguments.size();
    for (auto &a : v.arguments)
        ar << a->toString();
SERIALIZATION_SPLIT_END

////////////////////////////////////////

#define SERIALIZATION_TYPE ::sw::builder::Command
SERIALIZATION_BEGIN_SPLIT

    /*if (version != serialization_version)
    {
        throw SW_RUNTIME_ERROR("Incorrect archive version (" + std::to_string(version) + "), expected (" +
            std::to_string(serialization_version) + "), run configure command again");
    }*/

    ar & base_object<::primitives::Command>(v);

    ar & v.name;
    size_t flag;
    ar & flag;
    if (flag != 1)
        v.command_storage = (::sw::CommandStorage*)flag;
    else
        ar & v.command_storage_root;
    ar & v.first_response_file_argument;
    ar & v.always;
    ar & v.remove_outputs_before_execution;
    ar & v.strict_order;
    ar & v.output_dirs;

    ar & v.inputs;
    ar & v.outputs;
SERIALIZATION_SPLIT_CONTINUE
    ar & base_object<::primitives::Command>(v);

    ar & v.getName();
    if (!v.command_storage)
        ar & (size_t)v.command_storage;
    else
    {
        size_t x = 1;
        ar & x; // marker
        ar & v.command_storage->root;
    }
    ar & v.first_response_file_argument;
    ar & v.always;
    ar & v.remove_outputs_before_execution;
    ar & v.strict_order;
    ar & v.output_dirs;

    ar & v.inputs;
    ar & v.outputs;
SERIALIZATION_SPLIT_END

////////////////////////////////////////

/*
// no working currently
#define SERIALIZATION_TYPE ::sw::builder::BuiltinCommand
SERIALIZATION_BEGIN_SPLIT
    ar & base_object<::sw::builder::Command>(v);
SERIALIZATION_SPLIT_CONTINUE
    ar & base_object<::sw::builder::Command>(v);
SERIALIZATION_SPLIT_END*/

////////////////////////////////////////

#define SERIALIZATION_TYPE ::sw::Commands
SERIALIZATION_BEGIN_SPLIT
    size_t sz;
    ar >> sz;
    while (sz--)
    {
        auto c = std::make_shared<::sw::builder::Command>();
        v.insert(c);
        ar >> *c;
    }
SERIALIZATION_SPLIT_CONTINUE
    ar << v.size();
    for (const auto &c : v)
        ar << *c;
SERIALIZATION_SPLIT_END

////////////////////////////////////////

#define SERIALIZATION_TYPE ::sw::SimpleCommands
SERIALIZATION_BEGIN_SPLIT
    SW_UNREACHABLE;
SERIALIZATION_SPLIT_CONTINUE
    ar << v.size();
    for (const auto &c : v)
        ar << *c;
SERIALIZATION_SPLIT_END

// change when you update serialization
BOOST_CLASS_VERSION(::sw::builder::Command, 3)

////////////////////////////////////////////////////////////////////////////////
