// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4005) // warning C4005: 'XXX': macro redefinition
#endif

#pragma optimize("", off)

////////////////////////////////////////////////////////////////////////////////

#define SERIALIZATION_TYPE ::path
SERIALIZATION_BEGIN_SPLIT
    String s;
    ar >> s;
    v = fs::u8path(s);
SERIALIZATION_SPLIT_CONTINUE
    ar << v.u8string();
SERIALIZATION_SPLIT_END

////////////////////////////////////////

#define SERIALIZATION_TYPE Files
SERIALIZATION_BEGIN_SPLIT
    size_t sz;
    ar >> sz;
    while (sz--)
    {
        path p;
        ar >> p;
        v.insert(p);
    }
SERIALIZATION_SPLIT_CONTINUE
    ar << v.size();
    for (auto &p : v)
        ar << p;
SERIALIZATION_SPLIT_END

////////////////////////////////////////

#define SERIALIZATION_TYPE StringMap<String>
SERIALIZATION_BEGIN_SPLIT
    size_t sz;
    ar >> sz;
    while (sz--)
    {
        String k, va;
        ar >> k >> va;
        v[k] = va;
    }
SERIALIZATION_SPLIT_CONTINUE
    ar << v.size();
    for (auto &[k, va] : v)
        ar << k << va;
SERIALIZATION_SPLIT_END

////////////////////////////////////////

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
    ar & base_object<::primitives::Command>(v);

    ar & v.name;
    ar & v.command_storage;
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
    ar & v.command_storage;
    ar & v.first_response_file_argument;
    ar & v.always;
    ar & v.remove_outputs_before_execution;
    ar & v.strict_order;
    ar & v.output_dirs;

    ar & v.inputs;
    ar & v.outputs;
SERIALIZATION_SPLIT_END

////////////////////////////////////////

#define SERIALIZATION_TYPE ::std::unordered_set<std::shared_ptr<::sw::builder::Command>>
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
    SW_UNREACHABLE;
SERIALIZATION_SPLIT_END

////////////////////////////////////////

#define SERIALIZATION_TYPE ::std::vector<::sw::ExecutionPlan::PtrT>
SERIALIZATION_BEGIN_SPLIT
    SW_UNREACHABLE;
SERIALIZATION_SPLIT_CONTINUE
    ar << v.size();
    for (auto c : v)
        ar << *static_cast<::sw::builder::Command*>(c);
SERIALIZATION_SPLIT_END

////////////////////////////////////////////////////////////////////////////////
