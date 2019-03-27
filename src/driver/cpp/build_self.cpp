// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#define SW_PACKAGE_API
#include <sw/driver/cpp/sw.h>

#include <resolver.h>

#include <boost/algorithm/string.hpp>
#include <primitives/context.h>

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

void build_self(Solution &s)
{
#include <build_self.packages.generated.h>

    {
        SwapAndRestore store(getPackageStore(), PackageStore());

        // this provides initial download of driver dependencies
        Resolver r;
        r.add_downloads = false; // we hide our activity
        r.resolve_dependencies(required_packages);
        //auto pkgs = resolve_dependencies(required_packages);
        for (auto &p : r.getDownloadDependencies())
            s.knownTargets.insert(p);
    }

    s.Settings.Native.LibrariesType = LibraryType::Static;
    s.Variables["SW_SELF_BUILD"] = 1;

    SwapAndRestore sr(s.Local, false);
    build_self_generated(s);
}

} // namespace sw
