// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "inheritance.h"

#include <sw/core/settings.h>
#include <sw/core/target.h>
#include <sw/manager/package.h>
#include <sw/support/unresolved_package_id.h>

#include <memory>

namespace sw
{

struct ITarget;

struct SW_DRIVER_CPP_API DependencyData : IDependency
{
    bool Disabled = false;

    DependencyData(const ITarget &);
    DependencyData(const UnresolvedPackageId &);

    UnresolvedPackageId &getUnresolvedPackageId() override { return upkg; }
    const UnresolvedPackageId &getUnresolvedPackageId() const override { return upkg; }
    void setTarget(const ITarget &t) override;
    const ITarget &getTarget() const override;

    bool isDisabled() const { return Disabled; }

    //operator bool() const { return target; }
    bool isResolved() const override { return target; }

    const PackageName &getResolvedPackage() const;

    PackageSetting &getOption(const String &name) { return getOptions()[name]; }
    const PackageSetting &getOption(const String &name) const { return getOptions()[name]; }
    void setOption(const String &name, const PackageSetting &value) { getOption(name) = value; }

    PackageSettings &getOptions() { return getSettings()["options"].getMap(); }
    const PackageSettings &getOptions() const { return getSettings()["options"].getMap(); }

private:
    UnresolvedPackageId upkg;
    const ITarget *target = nullptr;
};

struct SW_DRIVER_CPP_API Dependency : DependencyData
{
    bool GenerateCommandsBefore = false; // do not make true by default
    bool IncludeDirectoriesOnly = false;
    bool LinkLibrariesOnly = false;

    using DependencyData::DependencyData;

    // for backwards compat
    void setDummy(bool) {}
};

using DependencyPtr = std::shared_ptr<Dependency>;

struct TargetDependency
{
    Dependency *dep = nullptr;
    InheritanceType inhtype;
};

}

namespace std
{

template<> struct hash<sw::DependencyData>
{
    size_t operator()(const sw::DependencyData &p) const
    {
        return std::hash<::sw::UnresolvedPackageId>()(p.getUnresolvedPackageId());
    }
};

template<> struct hash<sw::Dependency>
{
    size_t operator()(const sw::Dependency& p) const
    {
        return std::hash<::sw::UnresolvedPackageId>()(p.getUnresolvedPackageId());
    }
};

}
