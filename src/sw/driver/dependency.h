// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "inheritance.h"

#include <sw/core/settings.h>
#include <sw/manager/package.h>

#include <memory>

namespace sw
{

struct Target;

struct SW_DRIVER_CPP_API DependencyData
{
    // ITarget?
    Target *target = nullptr;
    UnresolvedPackage package;
    //DependencyData dep;
    //TargetSettings s;
    //InheritanceType type; // known in advance
    bool Disabled = false;

    DependencyData(const Target &t);
    DependencyData(const UnresolvedPackage &p);

    UnresolvedPackage getPackage() const;
    bool isDisabled() const { return Disabled; }

    bool operator==(const DependencyData &t) const;
    bool operator< (const DependencyData &t) const;
};

struct SW_DRIVER_CPP_API Dependency : DependencyData
{
    bool GenerateCommandsBefore = false; // do not make true by default

    // cpp (native) options
    bool IncludeDirectoriesOnly = false;

    // optional callback
    // choose default value
    std::function<void(Target &)> optional;

    using DependencyData::DependencyData;

    //Dependency &operator=(const Target &t);
    //Dependency &operator=(const Package *p);
    bool operator==(const Dependency &t) const;
    bool operator< (const Dependency &t) const;

    operator bool() const { return target; }
    bool isResolved() const { return operator bool(); }

    LocalPackage getResolvedPackage() const;
    void setTarget(const Target &t);

    // for backwards compat
    void setDummy(bool) {}
};

using DependencyPtr = std::shared_ptr<Dependency>;
//using DependenciesType = std::unordered_set<Dependency>;
//using DependenciesType = UniqueVector<DependencyPtr>;

struct SW_DRIVER_CPP_API TargetDependency
{
    DependencyPtr dep;
    TargetSettings settings;
    InheritanceType inhtype;

    //bool operator==(const TargetDependency &t) const { return std::tie(dep, s) == std::tie(t.dep, t.s); }
    //bool operator< (const TargetDependency &t) const { return std::tie(dep, s) <  std::tie(t.dep, t.s); }
    //bool operator==(const TargetDependency &t) const { return std::tie(package) == std::tie(t.package); }
    //bool operator< (const TargetDependency &t) const { return std::tie(package) <  std::tie(t.package); }

    /*TargetDependency &operator|=(const TargetDependency &t)
    {
        type |= t.type;
        return *this;
    }*/
};

}

namespace std
{

template<> struct hash<sw::DependencyData>
{
    size_t operator()(const sw::DependencyData &p) const
    {
        return std::hash<decltype(p.package)>()(p.package);
    }
};

template<> struct hash<sw::Dependency>
{
    size_t operator()(const sw::Dependency& p) const
    {
        return std::hash<decltype(p.package)>()(p.package);
    }
};

}
