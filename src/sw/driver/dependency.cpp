// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include "dependency.h"

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

const PackageName &DependencyData::getResolvedPackage() const
{
    if (!target)
        throw SW_RUNTIME_ERROR("Package is unresolved: " + getUnresolvedPackageId().getName().toString());
    return target->getPackage();
}

void DependencyData::setTarget(const ITarget &t)
{
    target = &t;
}

const ITarget &DependencyData::getTarget() const
{
    if (!target)
        throw SW_RUNTIME_ERROR("Package is unresolved: " + getUnresolvedPackageId().getName().toString());
    return *target;
}

} // namespace sw
