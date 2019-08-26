// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "concurrent_map.h"

#include <sw/builder/command.h>

#include <boost/thread/shared_mutex.hpp>
#include <primitives/templates.h>

namespace sw
{

namespace detail
{

struct Storage;

struct FileHolder
{
    ScopedFile f;
    path fn;

    FileHolder(const path &fn);
    ~FileHolder();
};

}

struct CommandRecord
{
    size_t hash = 0;
    fs::file_time_type mtime = fs::file_time_type::min();
    //Files implicit_inputs;
    std::unordered_set<size_t> implicit_inputs;

    Files getImplicitInputs(detail::Storage &) const;
    void setImplicitInputs(const Files &, detail::Storage &);
};

using ConcurrentCommandStorage = ConcurrentMap<size_t, CommandRecord>;
struct SwBuilderContext;

namespace detail
{

struct Storage
{
    ConcurrentCommandStorage storage;
    std::unique_ptr<FileHolder> commands;

    Files file_storage;
    mutable boost::upgrade_mutex m_file_storage_by_hash;
    std::unordered_map<size_t, path> file_storage_by_hash;
    std::unique_ptr<FileHolder> files;

    void closeLogs();
    FileHolder &getCommandLog(const SwBuilderContext &swctx, bool local);
    FileHolder &getFileLog(const SwBuilderContext &swctx, bool local);
};

}

struct FileDb
{
    const SwBuilderContext &swctx;

    FileDb(const SwBuilderContext &swctx);

    void load(Files &files, std::unordered_map<size_t, path> &files2, ConcurrentCommandStorage &commands, bool local) const;
    void save(const Files &files, const detail::Storage &, ConcurrentCommandStorage &commands, bool local) const;

    static void write(std::vector<uint8_t> &, const CommandRecord &, const detail::Storage &);
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
    detail::Storage &getInternalStorage(bool local);
    void async_command_log(const CommandRecord &r, bool local);

private:
    FileDb fdb;
    detail::Storage global;
    detail::Storage local;

    void closeLogs();
};

}
