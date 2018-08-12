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

#include <build_self.generated.h>

#define SW_SELF_BUILD
#define build build_self2
#include <../sw.cpp>

void check_self(Checker &c)
{
    check_self_generated(c);
}

void build_self(Solution &s)
{
    s.Settings.Native.LibrariesType = LibraryType::Static;

    {
        SwapAndRestore sr(s.Local, false);
        build_self_generated(s);
    }
    SwapAndRestore sr(s.Local, true);
    SwapAndRestore sr2(s.SourceDir, "d:/dev/cppan2/client");
    SwapAndRestore sr3(s.UseStorageBinaryDir, true);
    build_self2(s);
}
