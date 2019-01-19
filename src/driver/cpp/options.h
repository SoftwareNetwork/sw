// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "configuration.h"
#include "types.h"

#include <package.h>
#include <property.h>

#include <regex>
#include <unordered_set>
#include <unordered_map>
#include <variant>

namespace sw
{

namespace builder
{
struct Command;
}

struct NativeOptions;
struct NativeTarget;
struct Package;

using DefinitionKey = std::string;
using DefinitionValue = PropertyValue;
using VariableValue = PropertyValue;

struct DefinitionsType : std::map<DefinitionKey, VariableValue>
{
    using base = std::map<DefinitionKey, VariableValue>;

    base::mapped_type &operator[](const base::key_type &k)
    {
        if (!k.empty() && k.back() != '=')
            base::operator[](k);// = 1;
        return base::operator[](k);
    }

    // add operator+= ?
};

struct VariablesType : std::map<DefinitionKey, VariableValue>
{
    using base = std::map<DefinitionKey, VariableValue>;

    bool has(const typename base::key_type &k) const
    {
        return find(k) != end();
    }
};

template <class T>
class UniqueVector : public std::vector<T>
{
    using unique_type = std::unordered_set<T>;
    using base = std::vector<T>;

public:
    std::pair<typename base::iterator, bool> insert(const T &e)
    {
        auto[_, inserted] = u.insert(e);
        if (!inserted)
            return { base::end(), false };
        this->push_back(e);
        return { --base::end(), true };
    }

    template <class B, class E>
    void insert(const B &b, const E &e)
    {
        for (auto i = b; i != e; ++i)
            insert(*i);
    }

    void erase(const T &e)
    {
        if (!u.erase(e))
            return;
        base::erase(std::remove(base::begin(), base::end(), e), base::end());
    }

private:
    unique_type u;
};

struct FancyFilesOrdered : FilesOrdered
{
    using base = FilesOrdered;

    using base::insert;
    using base::erase;

    // fix return type
    void insert(const path &p)
    {
        push_back(p);
    }

    void erase(const path &p)
    {
        base::erase(std::remove(begin(), end(), p), end());
    }
};

//using PathOptionsType = FilesSorted;
using PathOptionsType = UniqueVector<path>;

struct SW_DRIVER_CPP_API Definition
{
    String d;

    Definition() = default;
    explicit Definition(const String &p);
};

struct SW_DRIVER_CPP_API Variable
{
    String v;
};

struct SW_DRIVER_CPP_API LinkLibrary
{
    String l;

    LinkLibrary() = default;
    explicit LinkLibrary(const String &p);
    explicit LinkLibrary(const path &p);
};

struct SW_DRIVER_CPP_API IncludeDirectory
{
    String i;

    IncludeDirectory() = default;
    explicit IncludeDirectory(const String &p);
    explicit IncludeDirectory(const path &p);
};

struct SW_DRIVER_CPP_API FileRegex
{
    path dir;
    std::regex r;
    bool recursive;

    FileRegex(const String &fn, bool recursive = false);
    FileRegex(const std::regex &r, bool recursive = false);
    FileRegex(const path &dir, const std::regex &r, bool recursive = false);
};

struct SW_DRIVER_CPP_API Dependency
{
    std::weak_ptr<NativeTarget> target;
    UnresolvedPackage package;
    std::vector<std::shared_ptr<Dependency>> chain;

    bool Disabled = false;
    bool GenerateCommandsBefore = false; // do not make true by default
    bool Dummy = false; // bool Runtime = false; ?

    // cpp (native) options
    bool IncludeDirectoriesOnly = false;
    bool WholeArchive = false;

    // optional callback
    // choose default value
    std::function<void(NativeTarget &)> optional;

    Dependency(const NativeTarget *t);
    Dependency(const UnresolvedPackage &p);

    Dependency &operator=(const NativeTarget *t);
    //Dependency &operator=(const Package *p);
    bool operator==(const Dependency &t) const;
    bool operator< (const Dependency &t) const;

    bool isDummy() const { return Disabled || Dummy; }

    //NativeTarget *get() const;
    //operator NativeTarget*() const;
    //NativeTarget *operator->() const;

    operator bool() const { return !!target.lock(); }
    bool isResolved() const { return operator bool(); }

    UnresolvedPackage getPackage() const;
    PackageId getResolvedPackage() const;

    void setTarget(const std::shared_ptr<NativeTarget> &t);
    void propagateTargetToChain();
};

using DependencyPtr = std::shared_ptr<Dependency>;
//using DependenciesType = std::unordered_set<Dependency>;
using DependenciesType = UniqueVector<DependencyPtr>;

struct SW_DRIVER_CPP_API NativeCompilerOptionsData
{
    DefinitionsType Definitions;
    Strings CompileOptions;
    PathOptionsType PreIncludeDirectories;
    PathOptionsType IncludeDirectories;
    PathOptionsType PostIncludeDirectories;

    PathOptionsType gatherIncludeDirectories() const;
    bool IsIncludeDirectoriesEmpty() const;
    void merge(const NativeCompilerOptionsData &o, const GroupSettings &s = GroupSettings(), bool merge_to_system = false);

    void add(const Definition &d);
    void remove(const Definition &d);
    void add(const DefinitionsType &defs);
    void remove(const DefinitionsType &defs);
};

using LinkLibrariesType = FancyFilesOrdered;

struct SW_DRIVER_CPP_API NativeLinkerOptionsData
{
    PathOptionsType Frameworks; // macOS
    // it is possible to have repeated link libraries on the command line
    LinkLibrariesType LinkLibraries;
    //PathOptionsType LinkLibraries2; // so on linux
    Strings LinkOptions;
    PathOptionsType PreLinkDirectories;
    PathOptionsType LinkDirectories;
    PathOptionsType PostLinkDirectories;

    PathOptionsType gatherLinkDirectories() const;
    LinkLibrariesType gatherLinkLibraries() const;
    bool IsLinkDirectoriesEmpty() const;
    void merge(const NativeLinkerOptionsData &o, const GroupSettings &s = GroupSettings());

    void add(const LinkLibrary &l);
    void remove(const LinkLibrary &l);
};

struct SW_DRIVER_CPP_API NativeCompilerOptions : IterableOptions<NativeCompilerOptions>,
    NativeCompilerOptionsData
{
    NativeCompilerOptionsData System;

    using NativeCompilerOptionsData::add;
    using NativeCompilerOptionsData::remove;

    void merge(const NativeCompilerOptions &o, const GroupSettings &s = GroupSettings());
    //void unique();

    void addDefinitionsAndIncludeDirectories(builder::Command &c) const;
    void addEverything(builder::Command &c) const;
    PathOptionsType gatherIncludeDirectories() const;
};

struct SW_DRIVER_CPP_API NativeLinkerOptions : IterableOptions<NativeLinkerOptions>,
    NativeLinkerOptionsData
{
    // 1. remove dups somewhere: implement unique_vector
    // 2. move Dependencies out - ???

    DependenciesType Dependencies;
    //Files FileDependencies; // this can be managed with existing source files
    NativeLinkerOptionsData System;

    using NativeLinkerOptionsData::add;
    using NativeLinkerOptionsData::remove;

    void add(const NativeTarget &t);
    void remove(const NativeTarget &t);

    void add(const DependencyPtr &t);
    void remove(const DependencyPtr &t);

    void add(const UnresolvedPackage &t);
    void remove(const UnresolvedPackage &t);

    void add(const UnresolvedPackages &t);
    void remove(const UnresolvedPackages &t);

    void add(const PackageId &t);
    void remove(const PackageId &t);

    void merge(const NativeLinkerOptions &o, const GroupSettings &s = GroupSettings());
    void addEverything(builder::Command &c) const;
    FilesOrdered gatherLinkLibraries() const;

    //
    //NativeLinkerOptions &operator+=(const NativeTarget &t);
    //NativeLinkerOptions &operator+=(const DependenciesType &t);
    //NativeLinkerOptions &operator+=(const Package &t);
    //NativeLinkerOptions &operator=(const DependenciesType &t);

    DependencyPtr operator+(const NativeTarget &t);
    DependencyPtr operator+(const DependencyPtr &d);
    DependencyPtr operator+(const PackageId &d);
};

using UnresolvedDependenciesType = std::unordered_map<UnresolvedPackage, DependencyPtr>;

struct SW_DRIVER_CPP_API NativeOptions : NativeCompilerOptions,
    NativeLinkerOptions,
    IterableOptions<NativeOptions>
{
    using NativeCompilerOptions::add;
    using NativeCompilerOptions::remove;
    using NativeLinkerOptions::add;
    using NativeLinkerOptions::remove;

    using IterableOptions<NativeOptions>::iterate;

    void merge(const NativeOptions &o, const GroupSettings &s = GroupSettings());
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

    template <class U>
    void inheritance(const InheritanceGroup<U> &g, const GroupSettings &s = GroupSettings())
    {
        // Private
        if (s.has_same_parent)
            Private.merge(g.Protected);
        Private.merge(g.Public);
        Private.merge(g.Interface);

        // Protected
        if (s.has_same_parent)
            Protected.merge(g.Protected);
        Protected.merge(g.Public);
        Protected.merge(g.Interface);

        // Public
        if (s.has_same_parent)
            Protected.merge(g.Protected);
        Public.merge(g.Public);
        Public.merge(g.Interface);

        // Interface, same as last in public
        //Public.merge(g.Interface);
    }

    template <class F, class ... Args>
    void iterate(F &&f, const GroupSettings &s = GroupSettings())
    {
        auto s2 = s;

        s2.Inheritance = InheritanceType::Private;
        //T::template iterate<F, Args...>(std::forward<F>(f), s);
        Private.template iterate<F, Args...>(std::forward<F>(f), s);
        s2.Inheritance = InheritanceType::Protected;
        Protected.template iterate<F, Args...>(std::forward<F>(f), s2);
        s2.Inheritance = InheritanceType::Public;
        Public.template iterate<F, Args...>(std::forward<F>(f), s2);
        s2.Inheritance = InheritanceType::Interface;
        Interface.template iterate<F, Args...>(std::forward<F>(f), s2);
    }

    template <class F, class ... Args>
    void iterate(F &&f, const GroupSettings &s = GroupSettings()) const
    {
        auto s2 = s;

        s2.Inheritance = InheritanceType::Private;
        //T::template iterate<F, Args...>(std::forward<F>(f), s);
        Private.template iterate<F, Args...>(std::forward<F>(f), s);
        s2.Inheritance = InheritanceType::Protected;
        Protected.template iterate<F, Args...>(std::forward<F>(f), s2);
        s2.Inheritance = InheritanceType::Public;
        Public.template iterate<F, Args...>(std::forward<F>(f), s2);
        s2.Inheritance = InheritanceType::Interface;
        Interface.template iterate<F, Args...>(std::forward<F>(f), s2);
    }

    // merge to T, always w/o interface
    void merge(const GroupSettings &s = GroupSettings())
    {
        T::merge(Private, s);
        T::merge(Protected, s);
        T::merge(Public, s);
    }

    // merge from other group, always w/ interface
    template <class U>
    void merge(const InheritanceGroup<U> &g, const GroupSettings &s = GroupSettings())
    {
        //T::merge(g.Private, s);
        T::merge(g.Protected, s);
        T::merge(g.Public, s);
        T::merge(g.Interface, s);

        /*T::merge(g, s);
        Private.merge(g.Private);
        Protected.merge(g.Protected);
        Public.merge(g.Public);
        Interface.merge(g.Interface);*/
    }

    InheritanceStorage<T> &getInheritanceStorage() { return data; }
    const InheritanceStorage<T> &getInheritanceStorage() const { return data; }
};

}

namespace std
{

template<> struct hash<sw::Dependency>
{
    size_t operator()(const sw::Dependency& p) const
    {
        return (size_t)p.target.lock().get();// ^ (size_t)p.package;
    }
};

template<> struct hash<sw::Definition>
{
    size_t operator()(const sw::Definition& d) const
    {
        return std::hash<decltype(d.d)>()(d.d);
    }
};

}
