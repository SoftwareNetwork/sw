// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2017-2020 Egor Pugin

#include "file_storage.h"

#include "file.h"

//#include <primitives/log.h>
//DECLARE_STATIC_LOGGER(logger, "file_storage");

namespace sw
{

FileData &FileStorage::registerFile(const path &in_f)
{
    auto p = normalize_path(in_f);
    auto d = files.insert(p);
    if (d.second)
        d.first->refresh(in_f);
    return *d.first;
}

}
