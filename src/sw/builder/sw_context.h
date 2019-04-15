// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "file_storage.h"
#include "os.h"

#include <sw/manager/sw_context.h>

struct Executor;

namespace sw
{

struct ProgramVersionStorage;

struct SW_BUILDER_API SwContext : SwManagerContext
{
    using FileDataHashMap = ConcurrentHashMap<path, FileData>;

    OS HostOS;

    SwContext(const path &local_storage_root_dir);
    virtual ~SwContext();

    FileStorage &getFileStorage(const String &config, bool local) const;
    FileStorage &getServiceFileStorage() const;
    Executor &getFileStorageExecutor() const;
    FileDataHashMap &getFileData() const;

private:
    using FileStorages = std::map<std::pair<bool, String>, std::unique_ptr<FileStorage>>;

    std::unique_ptr<ProgramVersionStorage> pvs;
    std::unique_ptr<FileDataHashMap> fshm; // before FileStorages!
    mutable FileStorages file_storages;
    std::unique_ptr<Executor> file_storage_executor; // after everything!
};

} // namespace sw
