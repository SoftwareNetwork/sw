// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

// TODO: RENAME THIS FILE?

#include "command_storage.h"
#include "file_storage.h"

#include <primitives/templates.h>

namespace sw
{

struct FileDb
{
    const SwContext &swctx;

    FileDb(const SwContext &swctx);

    void load(FileStorage &fs, ConcurrentHashMap<path, FileRecord> &files, bool local) const;
    void save(FileStorage &fs, ConcurrentHashMap<path, FileRecord> &files, bool local) const;
    void write(std::vector<uint8_t> &v, const FileRecord &r) const;

    void load(ConcurrentCommandStorage &commands, bool local) const;
    void save(ConcurrentCommandStorage &commands, bool local) const;
};

}
