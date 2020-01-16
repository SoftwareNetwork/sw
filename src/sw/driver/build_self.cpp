// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "entry_point.h"

#include <sw/core/sw_context.h>
#include <sw/core/target.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "build.self");

#define SW_PACKAGE_API
#include "sw.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4005) // warning C4005: 'XXX': macro redefinition
#endif

using TargetEntryPointMap = std::unordered_map<sw::PackageVersionGroupNumber, std::shared_ptr<sw::NativeBuiltinTargetEntryPoint>>;
using TargetEntryPointMap1 = std::unordered_map<sw::PackageId, std::shared_ptr<sw::NativeBuiltinTargetEntryPoint>>;

#define SW_DRIVER_ADD_SELF
#include <build_self.generated.h>

namespace sw
{

PackageIdSet load_builtin_packages(SwContext &swctx)
{
    static const UnresolvedPackages required_packages
    {
#include <build_self.packages.generated.h>
    };

    // create entry points by package
    //auto epm = build_self_generated();
    //for (auto &[gn, ep] : epm)
        //swctx.setEntryPoint(gn, ep); // still set

    //
    auto m = swctx.install(required_packages);

    PackageIdSet builtin_packages;
    for (auto &p : required_packages)
        builtin_packages.insert(m.find(p)->second);
    return builtin_packages;
}

} // namespace sw
