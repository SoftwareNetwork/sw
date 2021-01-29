// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include "dependency.h"

#include <sw/core/package.h>

namespace sw
{

DependencyData::DependencyData(const ITarget &t)
    : DependencyData(t.getPackage())
{
}

DependencyData::DependencyData(const UnresolvedPackageId &p)
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

void DependencyData::setTarget(std::unique_ptr<package_transform> t)
{
    transform = std::move(t);
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
