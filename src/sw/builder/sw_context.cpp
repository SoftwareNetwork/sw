// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "sw_context.h"

#include "file_storage.h"
#include "program_version_storage.h"

#include <sw/manager/storage.h>

#include <primitives/executor.h>

namespace sw
{

SwContext::SwContext(const path &local_storage_root_dir)
    : SwManagerContext(local_storage_root_dir)
{
    HostOS = getHostOS();
    //pvs = std::make_unique<ProgramVersionStorage>(getLocalStorage().storage_dir_tmp / "db" / "program_versions.txt");

    fshm = std::make_unique<FileDataHashMap>();

    // create service fs
    file_storages[{true, "service"}] = std::make_unique<FileStorage>(*this, "service");

    file_storage_executor = std::make_unique<Executor>("async log writer", 1);
}

SwContext::~SwContext() = default;

Executor &SwContext::getFileStorageExecutor() const
{
    return *file_storage_executor;
}

FileStorage &SwContext::getFileStorage(const String &config, bool local) const
{
    auto i = file_storages.find({ local, config });
    if (i != file_storages.end())
        return *i->second;
    std::lock_guard lk(m);
    file_storages[{ local, config }] = std::make_unique<FileStorage>(*this, config);
    return *file_storages[{ local, config }];
}

FileStorage &SwContext::getServiceFileStorage() const
{
    return *file_storages.find({true, "service"})->second;
}

SwContext::FileDataHashMap &SwContext::getFileData() const
{
    return *fshm;
}

}

