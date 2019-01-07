// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "enums.h"
#include "package_path.h"
#include "cppan_version.h"
#include "source.h"

#include <type_traits>
#include <unordered_set>
#include <unordered_map>

#define SW_SDIR_NAME "sdir"
#define SW_BDIR_NAME "bdir"
#define SW_BDIR_PRIVATE_NAME "bdir_pvt"

namespace sw
{

struct PackageId;
struct Package;
struct ExtendedPackageData;

struct SW_MANAGER_API UnresolvedPackage
{
    PackagePath ppath;
    VersionRange range;

    UnresolvedPackage() = default;
    UnresolvedPackage(const PackagePath &p, const VersionRange &r);
    UnresolvedPackage(const String &s);
    UnresolvedPackage(const PackageId &);

    String toString() const;
    bool canBe(const PackageId &id) const;

    /// return max satisfying package id
    ExtendedPackageData resolve() const;

    bool operator<(const UnresolvedPackage &rhs) const { return std::tie(ppath, range) < std::tie(rhs.ppath, rhs.range); }
    bool operator==(const UnresolvedPackage &rhs) const { return std::tie(ppath, range) == std::tie(rhs.ppath, rhs.range); }
    bool operator!=(const UnresolvedPackage &rhs) const { return !operator==(rhs); }
};

using UnresolvedPackages = std::unordered_set<UnresolvedPackage>;

struct SW_MANAGER_API PackageDescriptionInternal
{
    virtual ~PackageDescriptionInternal() = default;
    virtual std::tuple<path /* root dir */, Files> getFiles() const = 0;
    virtual UnresolvedPackages getDependencies() const = 0;

    // source
    // icons
    // screenshots, previews
    // desc: type, summary,
};

struct SW_MANAGER_API PackageId
{
    PackagePath ppath;
    Version version;

    PackageId() = default;
    // try to extract from string
    PackageId(const String &);
    PackageId(const PackagePath &, const Version &);
    /*PackageId(const PackageId &) = default;
    PackageId &operator=(const PackageId &) = default;*/

    PackagePath getPath() const { return ppath; }
    Version getVersion() const { return version; }

    path getDir() const;
    path getDirSrc() const;
    path getDirSrc2() const;
    path getDirObj() const;
    path getDirObjWdir() const;
    path getDirInfo() const;
    optional<path> getOverriddenDir() const;
    String getHash() const;
    String getHashShort() const;
    String getFilesystemHash() const;
    path getHashPath() const;
    path getHashPathSha256() const; // old, compat
    path getHashPathFull() const;
    path getStampFilename() const;
    String getStampHash() const;

    bool canBe(const PackageId &rhs) const;

    // delete?
    bool empty() const { return ppath.empty()/* || !version.isValid()*/; }

    bool operator<(const PackageId &rhs) const { return std::tie(ppath, version) < std::tie(rhs.ppath, rhs.version); }
    bool operator==(const PackageId &rhs) const { return std::tie(ppath, version) == std::tie(rhs.ppath, rhs.version); }
    bool operator!=(const PackageId &rhs) const { return !operator==(rhs); }

    // misc data
    /*String target_name;
    String target_name_hash;
    String variable_no_version_name;*/

    //void createNames();
    //String getTargetName() const;
    String getVariableName() const;

    Package toPackage() const;
    String toString() const;

    bool isPublic() const { return !isPrivate(); }
    bool isPrivate() const { return ppath.is_pvt() || ppath.is_com(); }

private:
    // cached vars
    //String hash;
    mutable String variable_name;

    path getDir(const path &p) const;
    static path getHashPathFromHash(const String &h);
};

//using PackagesId = std::unordered_map<String, PackageId>;
//using PackagesIdMap = std::unordered_map<PackageId, PackageId>;
using PackagesIdSet = std::unordered_set<PackageId>;

template <
    class T,
    template <class K, class V> class PackagePathMap,
    template <class K, class V> class VersionMap
>
struct PackageVersionMapBase : PackagePathMap<PackagePath, VersionMap<Version, T>>
{
    using VM = VersionMap<Version, T>;
    using Base = PackagePathMap<PackagePath, VM>;
    using This = PackageVersionMapBase;

    template <class U>
    struct Iterator
    {
        template <class T, class F>
        using cond_t = std::conditional_t<std::is_const_v<U>, T, F>;

        template <class T, class F>
        using cond_iterator_t = cond_t<typename T::const_iterator, typename F::iterator>;

        using reference = cond_t<const T&, T&>;
        using value_type = std::pair<const PackageId, reference>;

        using base_iterator = cond_iterator_t<typename U::Base, typename U::Base>;
        using vm_iterator = cond_iterator_t<typename U::VM, typename U::VM>;

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
        auto ip = find(pkg.ppath);
        if (ip == end(pkg.ppath))
            return end();
        auto iv = ip->second.find(pkg.version);
        if (iv == ip->second.end())
            return end();
        return { *this, ip, iv };
    }

    const_iterator find(const PackageId &pkg) const
    {
        auto ip = find(pkg.ppath);
        if (ip == end(pkg.ppath))
            return end();
        auto iv = ip->second.find(pkg.version);
        if (iv == ip->second.end())
            return end();
        return { *this, ip, iv };
    }

    iterator find(const UnresolvedPackage &u)
    {
        auto ip = find(u.ppath);
        if (ip == end(u.ppath))
            return end();
        std::set<Version> versions;
        for (const auto &[v, t] : ip->second)
            versions.insert(v);
        auto v = u.range.getMaxSatisfyingVersion(versions);
        if (!v)
            return end();
        return { *this, ip, ip->second.find(v.value()) };
    }

    const_iterator find(const UnresolvedPackage &u) const
    {
        auto ip = find(u.ppath);
        if (ip == end(u.ppath))
            return end();
        std::set<Version> versions;
        for (const auto &[v, t] : ip->second)
            versions.insert(v);
        auto v = u.range.getMaxSatisfyingVersion(versions);
        if (!v)
            return end();
        return { *this, ip, ip->second.find(v.value()) };
    }

    auto erase(const PackageId &pkg)
    {
        auto &v = ((PackageVersionMapBase*)this)->operator[](pkg.ppath);
        return v.erase(pkg.version);
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
        auto &v = ((PackageVersionMapBase*)this)->operator[](pkg.ppath);
        return v.emplace(pkg.version, val);
    }

    VM &operator[](const PackagePath &p)
    {
        return Base::operator[](p);
    }

    T &operator[](const PackageId &pkg)
    {
        return Base::operator[](pkg.ppath)[pkg.version];
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

    /*const T &operator[](const PackageId &pkg) const
    {
        return Base::operator[](pkg.ppath)[pkg.version];
    }*/
};

SW_MANAGER_API
UnresolvedPackage extractFromString(const String &target);

SW_MANAGER_API
PackageId extractFromStringPackageId(const String &target);

struct SW_MANAGER_API Package : PackageId
{
    SomeFlags flags;

    /*using PackageId::PackageId;
    using PackageId::operator=;
    Package(const PackageId &p) : PackageId(p) {}*/
};

using Packages = std::unordered_set<Package>;

/*struct CleanTarget
{
    enum Type
    {
        None = 0b0000'0000,

        Src = 0b0000'0001,
        Obj = 0b0000'0010,
        Lib = 0b0000'0100,
        Bin = 0b0000'1000,
        Exp = 0b0001'0000,
        Lnk = 0b0010'0000,

        All = 0xFF,
        AllExceptSrc = All & ~Src,
    };

    static std::unordered_map<String, int> getStrings();
    static std::unordered_map<int, String> getStringsById();
};

void cleanPackages(const String &s, int flags = CleanTarget::All);
void cleanPackages(const Packages &pkgs, int flags);*/

}

namespace std
{

template<> struct hash<::sw::PackageId>
{
    size_t operator()(const ::sw::PackageId &p) const
    {
        auto h = std::hash<::sw::PackagePath>()(p.ppath);
        return hash_combine(h, std::hash<::sw::Version>()(p.version));
    }
};

template<> struct hash<::sw::Package>
{
    size_t operator()(const ::sw::Package &p) const
    {
        return std::hash<::sw::PackageId>()(p);
    }
};

template<> struct hash<::sw::UnresolvedPackage>
{
    size_t operator()(const ::sw::UnresolvedPackage &p) const
    {
        auto h = std::hash<::sw::PackagePath>()(p.ppath);
        return hash_combine(h, std::hash<::sw::VersionRange>()(p.range));
    }
};

}
