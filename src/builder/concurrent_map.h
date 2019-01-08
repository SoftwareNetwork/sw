// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <junction/ConcurrentMap_Leapfrog.h>
#include <primitives/exceptions.h>
#include <primitives/templates.h>

#include <memory>

namespace sw
{

using ConcurrentContext = junction::QSBR::Context;

template <class K, class V>
struct ConcurrentMap
{
    using MapType = junction::ConcurrentMap_Leapfrog<K, V*>;
    //using MapType = std::unordered_map<K, V*>;
    using value_type = std::pair<K, V>;
    using insert_type = std::pair<V*, bool>;

    ConcurrentMap()
    {
        clear();
    }

    void clear()
    {
        map = std::make_unique<MapType>();
    }

    insert_type insert(const value_type &v)
    {
        return insert(v.first, v.second);
    }

    template <class Deleter>
    insert_type insert(K k, const V &v, Deleter &&d)
    {
        if (k == 0)
            throw SW_RUNTIME_ERROR("ConcurrentMap: zero key");

        //std::unique_lock lk(m);
        return insert_no_lock(k, v, d);
    }

    template <class Deleter>
    insert_type insert_no_lock(K k, const V &v, Deleter &&d)
    {
        if (k == 0)
            throw SW_RUNTIME_ERROR("ConcurrentMap: zero key");

        /*auto i = map->find(k);
        if (i == map->end())
        {
            auto value = new V(v);
            map->emplace(k, value);
            return { value, true };
        }
        return { i->second, false };*/

        auto i = map->insertOrFind(k);
        auto value = i.getValue();
        if (!value)
        {
            value = new V(v);
            auto oldValue = i.exchangeValue(value);
            if (oldValue)
            {
                *value = *oldValue;
                std::forward<Deleter>(d)(oldValue);
                return { value, false };
            }
            return { value, true };
        }
        return { value, false };
    }

    insert_type insert(K k, const V &v = V())
    {
        if constexpr (std::is_pointer_v<V>)
            return insert(k, v, [](auto *v)
        {
            junction::DefaultQSBR.enqueue(&V::destroy, v);
        });
        else
            return insert(k, v, [](auto *v) {});
    }

    insert_type insert_ptr(K k, const V &v = V())
    {
        if constexpr (std::is_pointer_v<V>)
            return insert(k, v, [](auto *v) { delete *v; });
        else
            return insert(k, v, [](auto *v) {});
    }

    V &operator[](K k)
    {
        return *insert(k).first;
    }

    const V &operator[](K k) const
    {
        return *insert(k).first;
    }

    auto getIterator()
    {
        return typename MapType::Iterator(*map);
    }

    auto begin() { return map->begin(); }
    auto end() { return map->end(); }

    auto begin() const { return map->begin(); }
    auto end() const { return map->end(); }

private:
    std::unique_ptr<MapType> map;
    //std::mutex m;
};

template <class V>
using ConcurrentMapSimple = ConcurrentMap<size_t, V>;

template <class K, class V>
struct ConcurrentHashMap : ConcurrentMapSimple<V>
{
    using Base = ConcurrentMapSimple<V>;
    using Base::operator[];
    using value_type = std::pair<K, V>;
    using insert_type = typename Base::insert_type;

    using Base::insert;

    insert_type insert(const K &k, const V &v = V())
    {
        return insert({ k, v });
    }

    insert_type insert(const value_type &v)
    {
        return Base::insert(std::hash<K>()(v.first), v.second);
    }

    V &operator[](const K &k)
    {
        return *insert(k).first;
    }

    const V &operator[](const K &k) const
    {
        return *insert(k).first;
    }
};

SW_BUILDER_API
SW_DECLARE_GLOBAL_STATIC_FUNCTION(ConcurrentContext, getConcurrentContext);

SW_BUILDER_API
ConcurrentContext createConcurrentContext();

SW_BUILDER_API
void destroyConcurrentContext(ConcurrentContext ctx);

SW_BUILDER_API
void updateConcurrentContext();

}
