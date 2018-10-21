// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <primitives/filesystem.h>

#include <package.h>

namespace sw
{

struct RunArgs
{
    PackageId pkg;
    path exe_path;
    Strings args;
    bool in_container = true;
};

void run(const RunArgs &args);

}
