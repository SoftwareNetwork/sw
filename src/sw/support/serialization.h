// Copyright (C) 2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <boost/serialization/split_free.hpp>

#define SERIALIZATION_BEGIN_SERIALIZE(t) \
    template <class Archive>             \
    void serialize(Archive &ar, t &v, const unsigned) {

#define SERIALIZATION_BEGIN_LOAD(t) \
    template <class Archive>        \
    void load(Archive &ar, t &v, const unsigned) {

#define SERIALIZATION_BEGIN_SAVE(t) \
    template <class Archive>        \
    void save(Archive &ar, const t &v, const unsigned) {

#define SERIALIZATION_BEGIN        \
    namespace boost::serialization \
    {

#define SERIALIZATION_BEGIN_SPLIT                      \
    BOOST_SERIALIZATION_SPLIT_FREE(SERIALIZATION_TYPE) \
    SERIALIZATION_BEGIN                                \
    SERIALIZATION_BEGIN_LOAD(SERIALIZATION_TYPE)

#define SERIALIZATION_BEGIN_UNIFIED \
    SERIALIZATION_BEGIN             \
    SERIALIZATION_BEGIN_SERIALIZE(SERIALIZATION_TYPE)

#define SERIALIZATION_END }
#define SERIALIZATION_SPLIT_CONTINUE } SERIALIZATION_BEGIN_SAVE(SERIALIZATION_TYPE)
#define SERIALIZATION_SPLIT_END } SERIALIZATION_END
#define SERIALIZATION_UNIFIED_END } SERIALIZATION_END

////////////////////////////////////////////////////////////////////////////////
//
// some common data
//
////////////////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4005)
#endif

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

#ifdef _MSC_VER
#pragma warning(pop)
#endif

////////////////////////////////////////////////////////////////////////////////
