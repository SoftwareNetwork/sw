// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
