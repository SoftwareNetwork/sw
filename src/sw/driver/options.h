// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "dependency.h"
#include "types.h"

#include <sw/builder/configuration.h>
#include <sw/manager/package.h>
#include <sw/manager/property.h>

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
struct Target;
struct Package;

using DefinitionKey = std::string;
//using DefinitionValue = PropertyValue;
using VariableValue = PropertyValue;
using DefinitionsType = std::map<DefinitionKey, VariableValue>;

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

struct SW_DRIVER_CPP_API Variable
{
    String v;
};

struct SW_DRIVER_CPP_API ApiNameType
{
    String a;

    ApiNameType() = default;
    explicit ApiNameType(const String &p);
};

struct SW_DRIVER_CPP_API Definition
{
    String d;

    Definition() = default;
    explicit Definition(const String &p);
};

struct SW_DRIVER_CPP_API Framework
{
    String f;

    Framework() = default;
    explicit Framework(const String &p);
    explicit Framework(const path &p);
};

struct SW_DRIVER_CPP_API IncludeDirectory
{
    String i;

    IncludeDirectory() = default;
    explicit IncludeDirectory(const String &p);
    explicit IncludeDirectory(const path &p);
};

struct SW_DRIVER_CPP_API LinkDirectory
{
    String d;

    LinkDirectory() = default;
    explicit LinkDirectory(const String &p);
    explicit LinkDirectory(const path &p);
};

struct SW_DRIVER_CPP_API LinkLibrary
{
    String l;

    LinkLibrary() = default;
    explicit LinkLibrary(const String &p);
    explicit LinkLibrary(const path &p);
};

struct SW_DRIVER_CPP_API SystemLinkLibrary
{
    String l;

    SystemLinkLibrary() = default;
    explicit SystemLinkLibrary(const String &p);
    explicit SystemLinkLibrary(const path &p);
};

struct SW_DRIVER_CPP_API PrecompiledHeader
{
    String h;

    PrecompiledHeader() = default;
    explicit PrecompiledHeader(const String &p);
    explicit PrecompiledHeader(const path &p);
};

struct SW_DRIVER_CPP_API FileRegex
{
    path dir;
    std::regex r;
    bool recursive;

    FileRegex(const String &r, bool recursive);
    FileRegex(const std::regex &r, bool recursive);
    FileRegex(const path &dir, const String &r, bool recursive);
    FileRegex(const path &dir, const std::regex &r, bool recursive);

    String getRegexString() const;

private:
    String regex_string;
};

using DependenciesType = UniqueVector<DependencyPtr>;

struct SW_DRIVER_CPP_API NativeCompilerOptionsData
{
    DefinitionsType Definitions;
    UniqueVector<String> CompileOptions;
    PathOptionsType PreIncludeDirectories;
    PathOptionsType IncludeDirectories;
    PathOptionsType PostIncludeDirectories;

    PathOptionsType gatherIncludeDirectories() const;
    bool IsIncludeDirectoriesEmpty() const;
    void merge(const NativeCompilerOptionsData &o, const GroupSettings &s = GroupSettings());

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
    LinkLibrariesType LinkLibraries2; // untouched link libs
    Strings LinkOptions;
    PathOptionsType PreLinkDirectories;
    PathOptionsType LinkDirectories;
    PathOptionsType PostLinkDirectories;
    PathOptionsType PrecompiledHeaders;

    PathOptionsType gatherLinkDirectories() const;
    LinkLibrariesType gatherLinkLibraries() const;
    bool IsLinkDirectoriesEmpty() const;
    void merge(const NativeLinkerOptionsData &o, const GroupSettings &s = GroupSettings());

    void add(const LinkDirectory &l);
    void remove(const LinkDirectory &l);

    void add(const LinkLibrary &l);
    void remove(const LinkLibrary &l);
};

struct SW_DRIVER_CPP_API NativeCompilerOptions : NativeCompilerOptionsData
{
    NativeCompilerOptionsData System;

    using NativeCompilerOptionsData::add;
    using NativeCompilerOptionsData::remove;

    void merge(const NativeCompilerOptions &o, const GroupSettings &s = GroupSettings());
    //void unique();

    void addDefinitions(builder::Command &c) const;
    void addIncludeDirectories(builder::Command &c) const;
    void addDefinitionsAndIncludeDirectories(builder::Command &c) const;
    void addCompileOptions(builder::Command &c) const;
    void addEverything(builder::Command &c) const;
    PathOptionsType gatherIncludeDirectories() const;
};

struct SW_DRIVER_CPP_API NativeLinkerOptions : NativeLinkerOptionsData
{
    NativeLinkerOptionsData System;

    // to use dyncasts
    virtual ~NativeLinkerOptions() = default;

    using NativeLinkerOptionsData::add;
    using NativeLinkerOptionsData::remove;

    void add(const SystemLinkLibrary &l);
    void remove(const SystemLinkLibrary &l);

    void merge(const NativeLinkerOptions &o, const GroupSettings &s = GroupSettings());
    void addEverything(builder::Command &c) const;
    FilesOrdered gatherLinkLibraries() const;

    //
    void add(const Target &t);
    void remove(const Target &t);

    void add(const DependencyPtr &t);
    void remove(const DependencyPtr &t);

    void add(const UnresolvedPackage &t);
    void remove(const UnresolvedPackage &t);

    void add(const UnresolvedPackages &t);
    void remove(const UnresolvedPackages &t);

    void add(const PackageId &t);
    void remove(const PackageId &t);

    DependencyPtr operator+(const ITarget &);
    DependencyPtr operator+(const DependencyPtr &);
    DependencyPtr operator+(const PackageId &);
    DependencyPtr operator+(const UnresolvedPackage &);

    const std::vector<DependencyPtr> &getRawDependencies() const { return deps; }

private:
    std::vector<DependencyPtr> deps;
};

using UnresolvedDependenciesType = std::unordered_map<UnresolvedPackage, DependencyPtr>;

struct SW_DRIVER_CPP_API NativeOptions : NativeCompilerOptions,
    NativeLinkerOptions
{
    using NativeCompilerOptions::add;
    using NativeCompilerOptions::remove;
    using NativeLinkerOptions::add;
    using NativeLinkerOptions::remove;

    void merge(const NativeOptions &o, const GroupSettings &s = GroupSettings());
};

}

namespace std
{

template<> struct hash<sw::Definition>
{
    size_t operator()(const sw::Definition& d) const
    {
        return std::hash<decltype(d.d)>()(d.d);
    }
};

}
