// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "storage.h"

#include <primitives/lock.h>

inline path get_lock(const path &fn)
{
    return sw::getStorage().storage_dir_etc / "locks" / fn.filename();
}
