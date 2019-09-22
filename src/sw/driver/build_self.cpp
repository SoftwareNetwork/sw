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

using TargetEntryPointMap = std::unordered_map<sw::PackageId, std::shared_ptr<sw::NativeBuiltinTargetEntryPoint>>;

#define SW_DRIVER_ADD_SELF
#include <build_self.generated.h>

namespace sw
{

PackageIdSet build_self(SwContext &swctx)
{
    static UnresolvedPackages required_packages
    {
#include <build_self.packages.generated.h>
    };

    // we need lock file here
    //static UnresolvedPackages store; // tmp store
    //auto m = s.swctx.install(required_packages, store);

    // create entry points by package
    auto epm1 = build_self_generated();

    //
    auto m = swctx.install(required_packages);

    // determine actual group numbers
    // on dev system overridden gn may be different from actual (remote) one
    std::unordered_map<sw::PackageVersionGroupNumber, std::shared_ptr<sw::NativeBuiltinTargetEntryPoint>> epm;
    for (auto &[p, ep] : epm1)
    {
        auto i = m.find(p);
        if (i == m.end())
            throw SW_RUNTIME_ERROR("Target not found: " + p.toString());
        auto gn = m.find(p)->second.getData().group_number;
        epm[gn] = ep;
    }

    // also set known pkgs
    PackageIdSet already_set;
    // spread entry points to other targets in group
    for (auto &[u, p] : m)
    {
        if (already_set.find(p) != already_set.end())
            continue;

        auto &ep = epm[p.getData().group_number];
        // may be empty when different versions is requested also
        // example:
        //  we request sqlite3-3.28.0
        //  some lib requests sqlite3-*
        //  now we got 3.28.0 and 3.29.0
        //  for 3.29.0 we do not have ep
        if (!ep)
        {
            LOG_WARN(logger, "Skipping package: " << p.toString());
            continue;
            // actually it's better throw here?
            //throw SW_RUNTIME_ERROR();
        }
        swctx.getTargetData(p).setEntryPoint(ep);

        already_set.insert(p);
    }
    return already_set;
}

} // namespace sw
