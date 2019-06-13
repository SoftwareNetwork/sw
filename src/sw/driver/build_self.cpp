// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#define SW_PACKAGE_API
#include "sw.h"

#include <sw/core/sw_context.h>
#include <sw/core/target.h>

// disable custom pragma warnings
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4005) // warning C4005: 'XXX': macro redefinition
#endif

using TargetEntryPointMap = std::unordered_map<sw::PackageVersionGroupNumber, std::shared_ptr<sw::NativeBuiltinTargetEntryPoint>>;

#include <build_self.generated.h>

namespace sw
{

void Build::build_self()
{
    static UnresolvedPackages required_packages
    {
#include <build_self.packages.generated.h>
    };

    // we need lock file here
    //static UnresolvedPackages store; // tmp store
    //auto m = s.swctx.install(required_packages, store);

    auto epm = build_self_generated(*this);

    auto m = swctx.install(required_packages);
    for (auto &[u, p] : m)
    {
        auto &ep = epm[p.getData().group_number];
        ep->module_data.known_targets.insert(p);
        getChildren()[p].setEntryPoint(ep);
    }
}

} // namespace sw
