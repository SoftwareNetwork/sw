// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#define SW_PACKAGE_API
#include <sw/driver/cpp/sw.h>

#include <directories.h>

#include <boost/algorithm/string.hpp>
#include <primitives/context.h>

#define SW_SELF_BUILD
#include <build_self.generated.h>

void check_self(Checker &c)
{
    check_self_generated(c);
}

void build_self(Solution &s)
{
#include <build_self.packages.generated.h>
    // this provides initial download of driver dependencies
    resolve_dependencies(required_packages);

    // then we must clean everything about them to prevent further resolve issues
    getPackageStore().clear();

    s.Settings.Native.LibrariesType = LibraryType::Static;

    SwapAndRestore sr(s.Local, false);
    build_self_generated(s);
}
