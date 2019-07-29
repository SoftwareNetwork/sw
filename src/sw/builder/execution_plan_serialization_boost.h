// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <boost/serialization/access.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4005) // warning C4005: 'XXX': macro redefinition
#endif

#pragma optimize("", off)

////////////////////////////////////////////////////////////////////////////////

#define SERIALIZATION_BEGIN_LOAD(t) \
    template <class Archive>        \
    void load(Archive &ar, t &v, const unsigned)

#define SERIALIZATION_BEGIN_SAVE(t) \
    template <class Archive>        \
    void save(Archive &ar, const t &v, const unsigned)

#define SERIALIZATION_BEGIN_SPLIT                      \
    BOOST_SERIALIZATION_SPLIT_FREE(SERIALIZATION_TYPE) \
    namespace boost::serialization                     \
    {                                                  \
    SERIALIZATION_BEGIN_LOAD(SERIALIZATION_TYPE)       \
    {

#define SERIALIZATION_END }
#define SERIALIZATION_SPLIT_CONTINUE } SERIALIZATION_BEGIN_SAVE(SERIALIZATION_TYPE) {
#define SERIALIZATION_SPLIT_END } SERIALIZATION_END

////////////////////////////////////////////////////////////////////////////////

#define SERIALIZATION_TYPE ::path
SERIALIZATION_BEGIN_SPLIT
    String s;
    ar >> s;
    v = fs::u8path(s);
SERIALIZATION_SPLIT_CONTINUE
    String s = v.u8string();
    ar << s;
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

namespace boost::serialization
{

template<class Archive>
void serialize(Archive &ar, ::sw::builder::Command::Argument &a, const unsigned)
{
    ar << a.toString();
}

} // namespace boost::serialization

    /*StringHashMap<int> gatherStrings() const
    {
        for (auto &c : commands)
        {
            insert(c->in.file.u8string());
            insert(c->out.file.u8string());
            insert(c->err.file.u8string());
        }
    }*/

////////////////////////////////////////

#define SERIALIZATION_TYPE ::sw::builder::Command
SERIALIZATION_BEGIN_SPLIT
    ar >> v.name;
    ar >> v.command_storage;
    ar >> v.working_directory;
    ar >> v.environment;
    size_t sz;
    ar >> sz;
    while (sz--)
    {
        String s;
        ar >> s;
        v.push_back(s);
    }
    ar >> v.inputs;
    ar >> v.outputs;
SERIALIZATION_SPLIT_CONTINUE
    ar << v.getName();
    ar << v.command_storage;
    ar << v.working_directory;
    ar << v.environment;
    ar << v.arguments.size();
    for (auto &a : v.arguments)
        ar << a->toString();
    ar << v.inputs;
    ar << v.outputs;
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
    SW_UNIMPLEMENTED;
SERIALIZATION_SPLIT_END

////////////////////////////////////////

#define SERIALIZATION_TYPE ::std::vector<::sw::ExecutionPlan::PtrT>
SERIALIZATION_BEGIN_SPLIT
    SW_UNIMPLEMENTED;
SERIALIZATION_SPLIT_CONTINUE
    ar << v.size();
    for (auto c : v)
        ar << *static_cast<::sw::builder::Command*>(c);
SERIALIZATION_SPLIT_END

////////////////////////////////////////////////////////////////////////////////
