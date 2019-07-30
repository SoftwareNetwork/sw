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
