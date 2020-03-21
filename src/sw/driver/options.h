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

template <class T>
struct FancyContainerOrdered : std::vector<T>
{
    using base = std::vector<T>;

    using base::insert;
    using base::erase;

    // fix return type
    void insert(const T &p)
    {
        push_back(p);
    }

    void erase(const T &p)
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
    path l;
    bool whole_archive = false;
    enum
    {
        NONE,
        MSVC,
        GNU,
    } style = NONE;

    LinkLibrary() = default;
    explicit LinkLibrary(const String &p);
    explicit LinkLibrary(const path &p);

    bool operator==(const LinkLibrary &rhs) const { return std::tie(l, whole_archive) == std::tie(rhs.l, rhs.whole_archive); }
};

struct SW_DRIVER_CPP_API SystemLinkLibrary
{
    path l;

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

    // other langs and targets may use this
    std::map<String, UniqueVector<String>> CustomTargetOptions;

    PathOptionsType gatherIncludeDirectories() const;
    bool IsIncludeDirectoriesEmpty() const;
    void merge(const NativeCompilerOptionsData &o, const GroupSettings &s = GroupSettings());

    void add(const Definition &d);
    void remove(const Definition &d);
    void add(const DefinitionsType &defs);
    void remove(const DefinitionsType &defs);
};

using FancyFilesOrdered = FancyContainerOrdered<path>;
using LinkLibrariesType = FancyContainerOrdered<LinkLibrary>;

struct SW_DRIVER_CPP_API NativeLinkerOptionsData
{
    // there are also -weak_framework s
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
    LinkLibrariesType gatherLinkLibraries() const;

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

    std::vector<DependencyPtr> &getRawDependencies() { return deps; }
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
