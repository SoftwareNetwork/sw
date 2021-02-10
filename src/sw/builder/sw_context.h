// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin

#pragma once

#include "concurrent_map.h"
#include "os.h"

#include <shared_mutex>

struct Executor;

namespace sw
{

struct CommandStorage;
struct FileStorage;

namespace builder::detail { struct ResolvableCommand; }

struct SW_BUILDER_API SwBuilderContext
{
    SwBuilderContext();
    ~SwBuilderContext();

    //FileStorage &getFileStorage() const;
    //Executor &getFileStorageExecutor() const;
    CommandStorage &getCommandStorage1(const path &root) const;

    //void clearFileStorages();
    void clearCommandStorages();

private:
    // keep order
    mutable std::unordered_map<path, std::unique_ptr<CommandStorage>> command_storages;
    //mutable std::unique_ptr<FileStorage> file_storage;
    //std::unique_ptr<Executor> file_storage_executor; // after everything!

    mutable std::mutex csm;
};

} // namespace sw
