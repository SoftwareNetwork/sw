// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2021 Egor Pugin <egor.pugin@gmail.com>

#include "unresolved_package_id.h"

#include "package_name.h"

namespace sw
{

UnresolvedPackageId::UnresolvedPackageId(const PackageName &n, const PackageSettings &s)
    : name(n), settings(s)
{
}

UnresolvedPackageId::UnresolvedPackageId(const UnresolvedPackageName &n, const PackageSettings &s)
    : name(n), settings(s)
{
}

UnresolvedPackageIdFull::UnresolvedPackageIdFull(const PackageName &n, const PackageSettings &s)
    : name(n), settings(s)
{
}

UnresolvedPackageIdFull::UnresolvedPackageIdFull(const UnresolvedPackageName &n, const PackageSettings &s)
    : name(n), settings(s)
{
}

}
