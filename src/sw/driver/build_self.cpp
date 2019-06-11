// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#define SW_PACKAGE_API
#include "sw.h"

#include <sw/core/sw_context.h>
#include <sw/core/target.h>

namespace sw
{

struct SW_DRIVER_CPP_API NativeBuiltinTargetEntryPoint : NativeTargetEntryPoint
{
    using BuildFunction = void(*)(Solution &);
    using CheckFunction = void(*)(Checker &);

    BuildFunction bf = nullptr;
    CheckFunction cf = nullptr;

    NativeBuiltinTargetEntryPoint(Build &b)
        : NativeTargetEntryPoint(b)
    {}

    void loadPackages(const PackageIdSet &pkgs = {}) override
    {
        SwapAndRestore sr1(b.knownTargets, pkgs);
        SwapAndRestore sr2(b.module_data, module_data);
        SwapAndRestore sr3(b.NamePrefix, module_data.NamePrefix);
        if (cf)
            cf(b.checker);
        if (!bf)
            throw SW_RUNTIME_ERROR("No internal build function set");
        bf(b);
    }
};

}

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

    //static UnresolvedPackages store; // tmp store
    //auto m = s.swctx.install(required_packages, store);

    SwapAndRestore sr(Local, false);
    auto epm = build_self_generated(*this);

    auto m = swctx.install(required_packages);
    for (auto &[u, p] : m)
    {
        auto &ep = epm[p.getData().group_number];
        ep->known_targets.insert(p);
        getChildren()[p].setEntryPoint(ep);
    }
}

} // namespace sw
