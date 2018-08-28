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

struct SW_BUILDER_API FileDataStorage
{
    ConcurrentHashMap<path, FileData> files;

    FileDataStorage();
    FileDataStorage(const FileDataStorage &) = delete;
    FileDataStorage &operator=(const FileDataStorage &) = delete;
    ~FileDataStorage();

    void load();
    void save();

    FileData *registerFile(const File &f);
    FileData *registerFile(const path &f);
};

struct SW_BUILDER_API FileStorage
{
    String config;
    ConcurrentHashMap<path, FileRecord> files;

    FileStorage(const String &config);
    FileStorage(const FileStorage &) = delete;
    FileStorage &operator=(const FileStorage &) = delete;
    ~FileStorage();

    void load();
    void save();
    void reset();

    FileRecord *registerFile(const File &f);
    FileRecord *registerFile(const path &f);
};

SW_BUILDER_API
FileDataStorage &getFileDataStorage();

SW_BUILDER_API
FileStorage &getFileStorage(const String &config);

}
