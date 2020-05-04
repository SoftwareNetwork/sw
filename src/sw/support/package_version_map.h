// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "package_id.h"

#include <type_traits>

namespace sw
{

template <
    class T,
    template <class ...> class PackagePathMap,
    template <class ...> class VersionMap
>
struct PackageVersionMapBase : PackagePathMap<PackagePath, VersionMap<T>>
{
    using version_map_type = VersionMap<T>;
    using Base = PackagePathMap<PackagePath, version_map_type>;
    using This = PackageVersionMapBase;

    template <class U>
    struct Iterator
    {
        template <class T2, class F>
        using cond_t = std::conditional_t<std::is_const_v<U>, T2, F>;

        template <class T2, class F>
        using cond_iterator_t = cond_t<typename T2::const_iterator, typename F::iterator>;

        using reference = cond_t<const T&, T&>;
        using value_type = std::pair<const PackageId, reference>;

        using base_iterator = cond_iterator_t<typename U::Base, typename U::Base>;
        using vm_iterator = cond_iterator_t<typename U::version_map_type, typename U::version_map_type>;

        U *t;
        base_iterator p;
        vm_iterator v;
        std::unique_ptr<value_type> ptr;

        Iterator(U &in)
            : t(&in)
        {
            p = t->Base::begin();
            if (p != t->Base::end())
            {
                v = p->second.begin();
                move_to_next(true);
            }
        }

        Iterator(U &in_u, base_iterator in_p)
            : t(&in_u), p(in_p)
        {
            if (p != t->Base::end())
            {
                v = p->second.begin();
                move_to_next(true);
            }
        }

        Iterator(U &in_u, base_iterator in_p, vm_iterator in_v)
            : t(&in_u), p(in_p), v(in_v)
        {
            if (p != t->Base::end())
                move_to_next(true);
        }

        value_type &operator*() { return *ptr; }
        const value_type &operator*() const { return *ptr; }

        auto operator->() { return ptr.get(); }
        auto operator->() const { return ptr.get(); }

        bool operator==(const Iterator &rhs) const
        {
            if (t != rhs.t)
                throw SW_RUNTIME_ERROR("Iterators refer to different maps");
            return p == rhs.p && (p == t->Base::end() || v == rhs.v);
        }

        bool operator!=(const Iterator &rhs) const
        {
            return !operator==(rhs);
        }

        Iterator &operator++()
        {
            move_to_next();
            return *this;
        }

    private:
        void set_ptr()
        {
            if (v != p->second.end())
                ptr = std::make_unique<value_type>(PackageId{ p->first, v->first }, v->second);
        }

        void move_to_next(bool init = false)
        {
            while (1)
            {
                if (p == t->Base::end())
                {
                    ptr.reset();
                    return;
                }
                if (v == p->second.end())
                {
                    ++p;
                    if (p == t->Base::end())
                        continue;
                    v = p->second.begin();
                }
                else if (!init)
                {
                    ++v;
                }
                if (v != p->second.end())
                    return set_ptr();
            }
        }
    };

    using iterator = Iterator<This>;
    using const_iterator = const Iterator<const This>;

    using Base::find;
    using Base::erase;
    //using Base::end; // use end(ppath) instead
    using Base::operator[];

    iterator find(const PackageId &pkg)
    {
        auto ip = find(pkg.getPath());
        if (ip == end(pkg.getPath()))
            return end();
        auto iv = ip->second.find(pkg.getVersion());
        if (iv == ip->second.end())
            return end();
        return { *this, ip, iv };
    }

    const_iterator find(const PackageId &pkg) const
    {
        auto ip = find(pkg.getPath());
        if (ip == end(pkg.getPath()))
            return end();
        auto iv = ip->second.find(pkg.getVersion());
        if (iv == ip->second.end())
            return end();
        return { *this, ip, iv };
    }

    iterator find(const UnresolvedPackage &u)
    {
        auto ip = find(u.getPath());
        if (ip == end(u.getPath()))
            return end();
        VersionSet versions;
        for (const auto &[v, t] : ip->second)
            versions.insert(v);
        auto v = u.range.getMaxSatisfyingVersion(versions);
        if (!v)
            return end();
        return { *this, ip, ip->second.find(v.value()) };
    }

    const_iterator find(const UnresolvedPackage &u) const
    {
        auto ip = find(u.getPath());
        if (ip == end(u.getPath()))
            return end();
        VersionSet versions;
        for (const auto &[v, t] : ip->second)
            versions.insert(v);
        auto v = u.range.getMaxSatisfyingVersion(versions);
        if (!v)
            return end();
        return { *this, ip, ip->second.find(v.value()) };
    }

    auto erase(const PackageId &pkg)
    {
        auto &v = ((PackageVersionMapBase*)this)->operator[](pkg.getPath());
        return v.erase(pkg.getVersion());
    }

    auto end(const PackageId &pkg)
    {
        return end();
    }

    auto end(const PackageId &pkg) const
    {
        return end();
    }

    auto end(const PackagePath &)
    {
        return Base::end();
    }

    auto end(const PackagePath &) const
    {
        return Base::end();
    }

    auto emplace(const PackageId &pkg, const T &val)
    {
        auto &v = ((PackageVersionMapBase*)this)->operator[](pkg.getPath());
        return v.emplace(pkg.getVersion(), val);
    }

    version_map_type &operator[](const PackagePath &p)
    {
        return Base::operator[](p);
    }

    T &operator[](const PackageId &pkg)
    {
        return Base::operator[](pkg.getPath())[pkg.getVersion()];
    }

    iterator begin()
    {
        return *this;
    }

    iterator end()
    {
        return { *this, Base::end() };
    }

    const_iterator begin() const
    {
        return *this;
    }

    const_iterator end() const
    {
        return { *this, Base::end() };
    }

    PackageIdSet getPackagesSet() const
    {
        PackageIdSet s;
        for (auto &[pkg, _] : *this)
            s.insert(pkg);
        return s;
    }
};

}
