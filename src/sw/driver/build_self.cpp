// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#define SW_PACKAGE_API
#include <sw/driver/sw.h>

#include <sw/core/sw_context.h>

// disable custom pragma warnings
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4005) // warning C4005: 'XXX': macro redefinition
#endif

#include <build_self.generated.h>

namespace sw
{

void check_self(Checker &c)
{
    check_self_generated(c);
}

void Build::build_self()
{
    static UnresolvedPackages required_packages
    {
#include <build_self.packages.generated.h>
    };

    //static UnresolvedPackages store; // tmp store
    //auto m = s.swctx.install(required_packages, store);

    auto m = swctx.install(required_packages);
    for (auto &[u, p] : m)
        knownTargets.insert(p);

    auto ss = createSettings();
    ss.Native.LibrariesType = LibraryType::Static;
    addSettings(ss);

    SwapAndRestore sr(Local, false);
    build_self_generated(*this);
}

} // namespace sw
