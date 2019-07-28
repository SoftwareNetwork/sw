// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <primitives/enums.h>
#include <primitives/exceptions.h>
#include <primitives/string.h>

#include <vector>

namespace sw
{

struct InheritanceScope
{
    enum
    {
        Package = 1 << 0,
        Project = 1 << 1, // consists of packages
        Other   = 1 << 2, // consists of projects and packages

        Private = Package,
        Group = Project,
        World = Other,
    };
};

enum class InheritanceType
{
    // 8 types
    // - 000 type (invalid)
    // = 7 types

    // 001 - usual private options
    Private = InheritanceScope::Package,

    // 011 - private and project
    Protected = InheritanceScope::Package | InheritanceScope::Project,

    // 111 - everyone
    Public = InheritanceScope::Package | InheritanceScope::Project | InheritanceScope::World,

    // 110 - project and others
    Interface = InheritanceScope::Project | InheritanceScope::World,

    // rarely used

    // 100 - only others
    // TODO: set new name?
    ProjectInterface = InheritanceScope::World,
    // or ProtectedInterface?

    // 010 - Project?
    // TODO: set new name
    ProjectOnly = InheritanceScope::Project,

    // 101 - package and others
    // TODO: set new name
    NotProject = InheritanceScope::Package | InheritanceScope::World,

    // alternative names

    Default = Private,
    Min = Private,
    Max = Public + 1,
};

inline InheritanceType operator|(InheritanceType lhs, InheritanceType rhs)
{
    using T = std::underlying_type_t<InheritanceType>;
    return static_cast<InheritanceType>(static_cast<T>(lhs) | static_cast<T>(rhs));
}

inline InheritanceType &operator|=(InheritanceType &lhs, InheritanceType rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

String toString(InheritanceType Type);

struct GroupSettings
{
    InheritanceType Inheritance = InheritanceType::Private;
    bool has_same_parent = false;
    bool merge_to_self = true;
    bool include_directories_only = false;
};

template <class T>
struct InheritanceStorage : std::vector<T*>
{
    using base = std::vector<T*>;

    InheritanceStorage(T *pvt)
        : base(toIndex(InheritanceType::Max), nullptr)
    {
        base::operator[](1) = pvt;
    }

    ~InheritanceStorage()
    {
        // we do not own 0 and 1 elements
        for (int i = toIndex(InheritanceType::Min) + 1; i < toIndex(InheritanceType::Max); i++)
            delete base::operator[](i);
    }

    T &operator[](int i)
    {
        auto &e = base::operator[](i);
        if (!e)
            e = new T;
        return *e;
    }

    const T &operator[](int i) const
    {
        return *base::operator[](i);
    }

    T &operator[](InheritanceType i)
    {
        return operator[](toIndex(i));
    }

    const T &operator[](InheritanceType i) const
    {
        return operator[](toIndex(i));
    }

    base &raw() { return *this; }
    const base &raw() const { return *this; }
};

/**
* \brief By default, group items considered as Private scope.
*/
template <class T>
struct InheritanceGroup : T
{
private:
    InheritanceStorage<T> data;

public:
    /**
    * \brief visible only in current target
    */
    T &Private;

    /**
    * \brief visible only in target and current project
    */
    T &Protected;
    // T &Project; ???

    /**
    * \brief visible both in target and its users
    */
    T &Public;

    /**
    * \brief visible in target's users
    */
    T &Interface;

    InheritanceGroup()
        : T()
        , data(this)
        , Private(*this)
        , Protected(data[InheritanceType::Protected])
        , Public(data[InheritanceType::Public])
        , Interface(data[InheritanceType::Interface])
    {
    }

    using T::operator=;

    T &get(InheritanceType Type)
    {
        switch (Type)
        {
        case InheritanceType::Private:
            return Private;
        case InheritanceType::Protected:
            return Protected;
        case InheritanceType::Public:
            return Public;
        case InheritanceType::Interface:
            return Interface;
        default:
            return data[Type];
        }
        throw SW_RUNTIME_ERROR("unreachable code");
    }

    const T &get(InheritanceType Type) const
    {
        switch (Type)
        {
        case InheritanceType::Private:
            return Private;
        case InheritanceType::Protected:
            return Protected;
        case InheritanceType::Public:
            return Public;
        case InheritanceType::Interface:
            return Interface;
        default:
            return data[Type];
        }
        throw SW_RUNTIME_ERROR("unreachable code");
    }

    template <class F>
    void iterate(F &&f) const
    {
        for (int i = toIndex(InheritanceType::Min); i < toIndex(InheritanceType::Max); i++)
        {
            auto s = getInheritanceStorage().raw()[i];
            if (s)
                f(*s, (InheritanceType)i);
        }
    }

    // merge to T, always w/o interface and always merge protected to self!
    void merge(const GroupSettings &s = GroupSettings())
    {
        T::merge(Protected, s);
        T::merge(Public, s);
    }

    // merge from other group, always w/ interface
    template <class U>
    void merge(const InheritanceGroup<U> &g, const GroupSettings &s = GroupSettings())
    {
        if (s.has_same_parent)
            T::merge(g.Protected, s);
        T::merge(g.Public, s);
        T::merge(g.Interface, s);
    }

    InheritanceStorage<T> &getInheritanceStorage() { return data; }
    const InheritanceStorage<T> &getInheritanceStorage() const { return data; }
};

}
