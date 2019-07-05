// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "concurrent_map.h"
#include "file.h"

#include <primitives/templates.h>

namespace sw
{

struct SwBuilderContext;

struct SW_BUILDER_API FileStorage
{
    const SwBuilderContext &swctx;
    ConcurrentHashMap<path, FileRecord> files;

    FileStorage(const SwBuilderContext &swctx);
    FileStorage(FileStorage &&) = default;
    FileStorage &operator=(FileStorage &&) = default;
    ~FileStorage();

    void clear();
    void reset();

    FileRecord *registerFile(const File &f);
    FileRecord *registerFile(const path &f);
};

}
