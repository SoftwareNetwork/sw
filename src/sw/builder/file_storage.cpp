// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin

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
