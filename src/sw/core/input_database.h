// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "input.h"

#include <sw/manager/database.h>

namespace sw
{

struct InputDatabase : Database
{
    InputDatabase(const path &p);

    void setupInput(Input &) const;

    // helper
    size_t addInputFile(const path &file) const;
};

} // namespace sw
