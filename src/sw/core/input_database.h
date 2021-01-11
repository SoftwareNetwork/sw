// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <sw/manager/database.h>

namespace sw
{

// we store:
//  - path
//  - contents hash
//  - last write time
struct SW_CORE_API InputDatabase : Database
{
    InputDatabase(const path &dbfn);

    size_t getFileHash(const path &) const;
};

} // namespace sw
