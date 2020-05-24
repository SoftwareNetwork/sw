// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2019 Egor Pugin <egor.pugin@gmail.com>

#include <boost/serialization/split_free.hpp>
// STL
#include <boost/serialization/deque.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/unique_ptr.hpp>
#include <boost/serialization/unordered_set.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/vector.hpp>
//
#include <primitives/exceptions.h>

#include <fstream>

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

////////////////////////////////////////

#define SERIALIZATION_TYPE ::path
SERIALIZATION_BEGIN_SPLIT
    String s;
    ar >> s;
    v = fs::u8path(s);
SERIALIZATION_SPLIT_CONTINUE
    ar << v.u8string();
SERIALIZATION_SPLIT_END

////////////////////////////////////////

#ifdef _MSC_VER
#pragma warning(pop)
#endif

////////////////////////////////////////////////////////////////////////////////

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

enum SerializationType
{
    BoostSerializationBinaryArchive,
    BoostSerializationTextArchive,
};

template <class T>
T deserialize(const path &archive_fn, int type = 0)
{
    T data;
    if (type == SerializationType::BoostSerializationBinaryArchive)
    {
        std::ifstream ifs(archive_fn, std::ios_base::in | std::ios_base::binary);
        if (!ifs)
            throw SW_RUNTIME_ERROR("Cannot read file: " + normalize_path(archive_fn));
        boost::archive::binary_iarchive ia(ifs);
        load(ia, data);
    }
    else if (type == SerializationType::BoostSerializationTextArchive)
    {
        std::ifstream ifs(archive_fn);
        if (!ifs)
            throw SW_RUNTIME_ERROR("Cannot read file: " + normalize_path(archive_fn));
        boost::archive::text_iarchive ia(ifs);
        load(ia, data);
    }
    else
        throw SW_RUNTIME_ERROR("Bad type");
    return data;
}

template <class T>
void serialize(const path &archive_fn, const T &v, int type = 0)
{
    if (type == SerializationType::BoostSerializationBinaryArchive)
    {
        std::ofstream ofs(archive_fn, std::ios_base::out | std::ios_base::binary);
        if (!ofs)
            throw SW_RUNTIME_ERROR("Cannot write file: " + normalize_path(archive_fn));
        boost::archive::binary_oarchive oa(ofs);
        return save(oa, v);
    }
    else if (type == SerializationType::BoostSerializationTextArchive)
    {
        std::ofstream ofs(archive_fn);
        if (!ofs)
            throw SW_RUNTIME_ERROR("Cannot write file: " + normalize_path(archive_fn));
        boost::archive::text_oarchive oa(ofs);
        return save(oa, v);
    }
    else
        throw SW_RUNTIME_ERROR("Bad type");
}
