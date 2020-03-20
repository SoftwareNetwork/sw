/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2020 Egor Pugin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

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
ENABLE_ENUM_CLASS_BITMASK(InheritanceType);

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
        return data[Type];
    }

    const T &get(InheritanceType Type) const
    {
        return data[Type];
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

    template <class F>
    void iterate_this(F &&f) const
    {
        for (int i = toIndex(InheritanceType::Min); i < toIndex(InheritanceType::Max); i++)
        {
            if ((i & InheritanceScope::Package) == 0)
                continue;
            auto s = getInheritanceStorage().raw()[i];
            if (s)
                f(*s, (InheritanceType)i);
        }
    }

    // merge self to self, always w/o interface and always merge protected to self!
    void merge()
    {
        T::merge(Protected);
        T::merge(Public);
    }

    // merge from other group, always w/ interface
    template <class U>
    void merge(const InheritanceGroup<U> &g, const GroupSettings &in)
    {
        auto s = in;
        s.merge_to_self = false;
        if (s.has_same_parent)
            T::merge(g.Protected, s);
        T::merge(g.Public, s);
        T::merge(g.Interface, s);
    }

    InheritanceStorage<T> &getInheritanceStorage() { return data; }
    const InheritanceStorage<T> &getInheritanceStorage() const { return data; }
};

}
