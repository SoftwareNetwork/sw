// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "concurrent_map.h"

#include <sw/builder/command.h>

#include <boost/thread/shared_mutex.hpp>
#include <primitives/lock.h>
#include <primitives/templates.h>

#include <atomic>

namespace sw
{

namespace detail
{

struct Storage;

struct FileHolder
{
    //ScopedFileLock lk;
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
    FileHolder &getCommandLog(const SwBuilderContext &swctx, const path &root);
    FileHolder &getFileLog(const SwBuilderContext &swctx, const path &root);
};

}

struct FileDb
{
    const SwBuilderContext &swctx;

    FileDb(const SwBuilderContext &swctx);

    void load(Files &files, std::unordered_map<size_t, path> &files2, ConcurrentCommandStorage &commands, const path &root) const;
    void save(const Files &files, const detail::Storage &, ConcurrentCommandStorage &commands, const path &root) const;

    static void write(std::vector<uint8_t> &, const CommandRecord &, const detail::Storage &);
};

struct SW_BUILDER_API CommandStorage
{
    const SwBuilderContext &swctx;
    path root;

    CommandStorage(const SwBuilderContext &swctx, const path &root);
    CommandStorage(const CommandStorage &) = delete;
    CommandStorage &operator=(const CommandStorage &) = delete;
    ~CommandStorage();

    void load();
    void save();

    ConcurrentCommandStorage &getStorage();
    detail::Storage &getInternalStorage();
    void async_command_log(const CommandRecord &r);
    void add_user();
    void free_user();
    std::pair<CommandRecord *, bool> insert(size_t hash);

private:
    FileDb fdb;
    detail::Storage s;
    std::atomic_int n_users;

    void closeLogs();
};

}
