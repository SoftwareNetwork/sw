// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2017-2020 Egor Pugin

#pragma once

#include "concurrent_map.h"

#include <primitives/filesystem.h>

namespace sw
{

struct FileData;

struct SW_BUILDER_API FileStorage
{
    FileData &registerFile(const path &f);

private:
    using FileDataHashMap = ConcurrentHashMap<path, FileData>;
    FileDataHashMap files;
};

}
