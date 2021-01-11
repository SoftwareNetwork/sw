// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin

#pragma once

#include "concurrent_map.h"

#include <primitives/filesystem.h>

namespace sw
{

struct FileData;
struct SwBuilderContext;

struct SW_BUILDER_API FileStorage
{
    using FileDataHashMap = ConcurrentHashMap<path, FileData>;

    FileDataHashMap files;

    void clear(); // remove?
    void reset(); // remove?

    FileData &registerFile(const path &f);
};

}
