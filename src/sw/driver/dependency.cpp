// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include "dependency.h"

#include <sw/core/package.h>

namespace sw
{

IDependency::~IDependency() = default;

DependencyData::DependencyData(const ITarget &t)
    : DependencyData(t.getPackage())
{
}

DependencyData::DependencyData(const UnresolvedPackageIdFull &p)
    : upkg(p)
{
}

DependencyData::~DependencyData()
{
}

/*const PackageName &DependencyData::getResolvedPackage() const
{
    if (!target)
        throw SW_RUNTIME_ERROR("Package is unresolved: " + getUnresolvedPackageId().getName().toString());
    return target->getPackage();
}

void DependencyData::setTarget(const ITarget &t)
{
    target = &t;
}*/

void DependencyData::setTarget(const package_transform &t)
{
    transform = &t;
}

/*const ITarget &DependencyData::getTarget() const
{
    if (!target)
        throw SW_RUNTIME_ERROR("Package is unresolved: " + getUnresolvedPackageId().getName().toString());
    return *target;
}*/

const PackageSettings &DependencyData::getInterfaceSettings() const
{
    if (transform)
        return transform->get_properties();
    SW_UNIMPLEMENTED;
}

} // namespace sw
