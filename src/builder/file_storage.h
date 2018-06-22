// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "concurrent_map.h"
#include "file.h"

namespace sw
{

struct SW_BUILDER_API FileStorage
{
    ConcurrentHashMap<path, FileRecord> files;

    FileStorage();
    FileStorage(const FileStorage &) = delete;
    FileStorage &operator=(const FileStorage &) = delete;
    ~FileStorage();

    void load();
    void save();
    void reset();

    void registerFile(const File &f);
};

}
