// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "command_storage.h"
#include "file_storage.h"

namespace sw
{

struct Db
{
    virtual void load(FileStorage &fs, ConcurrentHashMap<path, FileRecord> &files) const = 0;
    virtual void save(FileStorage &fs, ConcurrentHashMap<path, FileRecord> &files) const = 0;
    virtual void write(std::vector<uint8_t> &v, const FileRecord &r) const {}

    virtual void load(ConcurrentCommandStorage &commands) const = 0;
    virtual void save(ConcurrentCommandStorage &commands) const = 0;

    //virtual void load(const path &fn, ChecksContainer &checks) const = 0;
    //virtual void save(const path &fn, const ChecksContainer &checks) const = 0;
};

Db &getDb();

}
