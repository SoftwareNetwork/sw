// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "execution_plan.h"

#include <nlohmann/json.hpp>
#include <primitives/exceptions.h>

#include <boost/serialization/access.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

#include <fstream>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4005) // warning C4005: 'XXX': macro redefinition
#endif

//#pragma optimize("", off)

////////////////////////////////////////////////////////////////////////////////

#define SERIALIZATION_BEGIN_LOAD(t) \
    template <class Archive>        \
    void load(Archive &ar, t &v, const unsigned int)

#define SERIALIZATION_BEGIN_SAVE(t) \
    template <class Archive>        \
    void save(Archive &ar, const t &v, const unsigned int)

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
    ar << v.u8string();
SERIALIZATION_SPLIT_END

#define SERIALIZATION_TYPE StringMap<String>
SERIALIZATION_BEGIN_SPLIT
    size_t sz;
    ar >> sz;
    while (sz--)
    {
        String k, v;
        ar >> k >> v;
        p[k] = v;
    }
SERIALIZATION_SPLIT_CONTINUE
    ar << v.size();
    for (auto &[k, va] : v)
        ar << k << va;
SERIALIZATION_SPLIT_END

namespace boost::serialization
{

template<class Archive>
void serialize(Archive &ar, ::sw::builder::Command::Argument &a, const unsigned int)
{
    ar << a.toString();
}

template<class Archive>
void serialize(Archive &ar, ::sw::builder::Command &c, const unsigned int)
{
    ar << c.working_directory;
    ar << c.environment;
    for (auto &a : c.getArguments())
        ar << *a;
}

} // namespace boost::serialization

#define SERIALIZATION_TYPE ::sw::ExecutionPlan
SERIALIZATION_BEGIN_SPLIT
    SW_UNIMPLEMENTED;
SERIALIZATION_SPLIT_CONTINUE
    for (auto c : v.getCommands())
        ar << *static_cast<::sw::builder::Command*>(c);
SERIALIZATION_SPLIT_END

namespace sw
{

enum SerializationType
{
    BoostSerializationTextArchive =     0,
    BoostSerializationBinaryArchive =   1,
};

void ExecutionPlan::load(const path &p, int type)
{

}

void ExecutionPlan::save(const path &p, int type) const
{
    std::ofstream ofs(p);
    boost::archive::text_oarchive oa(ofs);
    oa << *this;
}

}
