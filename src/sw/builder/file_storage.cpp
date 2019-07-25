// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "file_storage.h"

#include "file.h"
#include "sw_context.h"

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "file_storage");

namespace sw
{

void FileStorage::clear()
{
    files.clear();
}

void FileStorage::reset()
{
    for (const auto &[k, f] : files)
        f.reset();
}

FileData &FileStorage::registerFile(const path &in_f)
{
    auto p = normalize_path(in_f);
    auto d = files.insert(p);
    if (d.second)
        d.first->refresh(in_f);
    return *d.first;
}

}
