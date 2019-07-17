// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "inheritance.h"

#include <sw/core/settings.h>
#include <sw/core/target.h>
#include <sw/manager/package.h>

#include <memory>

namespace sw
{

struct ITarget;

struct SW_DRIVER_CPP_API DependencyData : IDependency
{
    UnresolvedPackage package;
    TargetSettings settings;
    bool Disabled = false;

    DependencyData(const ITarget &t);
    DependencyData(const UnresolvedPackage &p);

    UnresolvedPackage getPackage() const;
    UnresolvedPackage getUnresolvedPackage() const override { return getPackage(); }
    void setTarget(const ITarget &t) override;
    const ITarget &getTarget() const;

    bool isDisabled() const { return Disabled; }

    operator bool() const { return target; }
    bool isResolved() const override { return operator bool(); }

    const TargetSettings &getSettings() const override { return settings; }

    PackageId getResolvedPackage() const;

private:
    const ITarget *target = nullptr;
};

struct SW_DRIVER_CPP_API Dependency : DependencyData
{
    bool GenerateCommandsBefore = false; // do not make true by default
    bool IncludeDirectoriesOnly = false;

    using DependencyData::DependencyData;

    // for backwards compat
    void setDummy(bool) {}
};

using DependencyPtr = std::shared_ptr<Dependency>;

struct SW_DRIVER_CPP_API TargetDependency
{
    DependencyPtr dep;
    InheritanceType inhtype;
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
