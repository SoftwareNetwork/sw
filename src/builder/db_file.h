// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "db.h"

namespace sw
{

struct FileDb : Db
{
    FileDb(const path &fn);

    void load(ConcurrentHashMap<path, FileData> &files) const override;
    void save(ConcurrentHashMap<path, FileData> &files) const override;
    void load(const String &config, ConcurrentHashMap<path, FileRecord> &files) const override;
    void save(const String &config, ConcurrentHashMap<path, FileRecord> &files) const override;
    void write(std::vector<uint8_t> &v, const FileData &r) const override;
    void write(std::vector<uint8_t> &v, const FileRecord &r) const override;

    void load(ConcurrentCommandStorage &commands) const override;
    void save(ConcurrentCommandStorage &commands) const override;
};

}
