// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include "builtin_input.h"
#include "entry_point.h"

#include <sw/core/sw_context.h>
#include <sw/core/target.h>

#include <primitives/executor.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "build.self");

#define SW_PACKAGE_API
#include "sw.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4005) // warning C4005: 'XXX': macro redefinition
#endif

#include <build_self.generated.h>

namespace sw
{

PackageIdSet load_builtin_packages(SwContext &swctx)
{
    static const UnresolvedPackages required_packages
    {
#include <build_self.packages.generated.h>
    };

    PackageIdSet builtin_packages;
    for (auto &p : required_packages)
    {
        ResolveRequest rr{p};
        swctx.resolve(rr);
        if (!rr.isResolved())
            throw SW_RUNTIME_ERROR("Cannot resolve: " + p.toString());
        builtin_packages.insert(rr.getPackage());
    }

    // mass (threaded) install!
    auto &e = getExecutor();
    Futures<void> fs;
    for (auto &p : builtin_packages)
    {
        fs.push_back(e.push([&swctx, &p]
        {
            ResolveRequest rr{p};
            swctx.install(rr);
        }));
    }
    waitAndGet(fs);

    return builtin_packages;
}

} // namespace sw
