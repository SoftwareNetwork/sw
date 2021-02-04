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
struct package_transform;

struct SW_DRIVER_CPP_API IDependency
{
    virtual ~IDependency() = 0;

    virtual UnresolvedPackageId &getUnresolvedPackageId() = 0;
    virtual const UnresolvedPackageId &getUnresolvedPackageId() const = 0;

    virtual bool isResolved() const = 0;
    //virtual void setTarget(const ITarget &) = 0;
    //virtual const ITarget &getTarget() const = 0;

    //PackageSettings &getSettings();
    //const PackageSettings &getSettings() const;
};

using IDependencyPtr = std::shared_ptr<IDependency>;

struct SW_DRIVER_CPP_API DependencyData : IDependency
{
    bool Disabled = false;

    DependencyData(const ITarget &);
    DependencyData(const UnresolvedPackageId &);
    ~DependencyData();

    UnresolvedPackageId &getUnresolvedPackageId() override { return upkg; }
    const UnresolvedPackageId &getUnresolvedPackageId() const override { return upkg; }
    //void setTarget(const ITarget &t) override;
    void setTarget(std::shared_ptr<package_transform>);
    //const ITarget &getTarget() const override;
    // get properties
    const PackageSettings &getInterfaceSettings() const;

    bool isDisabled() const { return Disabled; }

    //operator bool() const { return target; }
    bool isResolved() const override { return /*target || */!!transform; }

    //const PackageName &getResolvedPackage() const;

    PackageSetting &getOption(const String &name) { return getOptions()[name]; }
    const PackageSetting &getOption(const String &name) const { return getOptions()[name]; }
    void setOption(const String &name, const PackageSetting &value) { getOption(name) = value; }

    PackageSettings &getOptions() { return getSettings()["options"].getMap(); }
    const PackageSettings &getOptions() const { return getSettings()["options"].getMap(); }

    PackageSettings &getSettings() { return getUnresolvedPackageId().getSettings(); }
    const PackageSettings &getSettings() const { return getUnresolvedPackageId().getSettings(); }

private:
    UnresolvedPackageId upkg;
public:
    std::shared_ptr<package_transform> transform;
    //const ITarget *target = nullptr;
};

struct SW_DRIVER_CPP_API Dependency : DependencyData
{
    bool GenerateCommandsBefore = false; // do not make true by default
    bool IncludeDirectoriesOnly = false;
    bool LinkLibrariesOnly = false;
    std::shared_ptr<Package> resolved_pkg;

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
