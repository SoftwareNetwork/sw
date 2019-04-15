// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <sw/manager/package.h>

#include <primitives/filesystem.h>
#include <primitives/command.h>

namespace sw
{

void run(const LocalPackage &pkg, primitives::Command &c);

}
