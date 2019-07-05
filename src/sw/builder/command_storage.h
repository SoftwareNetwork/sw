// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "concurrent_map.h"

#include <primitives/templates.h>

#include <sw/builder/command.h>

namespace sw
{

struct CommandRecord
{
    size_t hash = 0;
    size_t mtime = 0;
    Files implicit_inputs;
};

using ConcurrentCommandStorage = ConcurrentMap<size_t, CommandRecord>;
struct SwBuilderContext;

struct FileDb
{
    const SwBuilderContext &swctx;

    FileDb(const SwBuilderContext &swctx);

    void load(Files &files, ConcurrentCommandStorage &commands, bool local) const;
    void save(Files &files, ConcurrentCommandStorage &commands, bool local) const;

    static void write(std::vector<uint8_t> &, const CommandRecord &);
};

struct SW_BUILDER_API CommandStorage
{
    const SwBuilderContext &swctx;

    CommandStorage(const SwBuilderContext &swctx);
    CommandStorage(const CommandStorage &) = delete;
    CommandStorage &operator=(const CommandStorage &) = delete;
    ~CommandStorage();

    void load();
    void save();

    ConcurrentCommandStorage &getStorage(bool local);
    void async_command_log(const CommandRecord &r, bool local);

private:
    struct FileHolder
    {
        ScopedFile f;
        path fn;

        FileHolder(const path &fn);
        ~FileHolder();
    };

    struct Storage
    {
        ConcurrentCommandStorage storage;
        std::unique_ptr<FileHolder> commands;

        Files file_storage;
        std::unique_ptr<FileHolder> files;

        void closeLogs();
        FileHolder &getCommandLog(const SwBuilderContext &swctx, bool local);
        FileHolder &getFileLog(const SwBuilderContext &swctx, bool local);
    };

    FileDb fdb;
    Storage global;
    Storage local;

    Storage &getStorage1(bool local);
    void closeLogs();
};

}
