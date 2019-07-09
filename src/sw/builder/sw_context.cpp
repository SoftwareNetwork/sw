// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "sw_context.h"

#include "command_storage.h"
#include "file_storage.h"
#include "program_version_storage.h"

#include <sw/manager/storage.h>

#include <primitives/executor.h>

namespace sw
{

SwBuilderContext::SwBuilderContext(const path &local_storage_root_dir)
    : SwManagerContext(local_storage_root_dir)
{
    HostOS = getHostOS();

    //
    file_storage_executor = std::make_unique<Executor>("async log writer", 1);

    //
    pvs = std::make_unique<ProgramVersionStorage>(getLocalStorage().storage_dir_tmp / "db" / "program_versions.txt");
}

SwBuilderContext::~SwBuilderContext()
{
}

Executor &SwBuilderContext::getFileStorageExecutor() const
{
    return *file_storage_executor;
}

FileStorage &SwBuilderContext::getFileStorage() const
{
    if (!file_storage)
        file_storage = std::make_unique<FileStorage>();
    return *file_storage;
}

CommandStorage &SwBuilderContext::getCommandStorage() const
{
    if (!cs)
    {
        std::unique_lock lk(csm);
        if (!cs)
            cs = std::make_unique<CommandStorage>(*this);
    }
    return *cs;
}

ProgramVersionStorage &SwBuilderContext::getVersionStorage() const
{
    return *pvs;
}

void SwBuilderContext::clearFileStorages()
{
    file_storage.reset();
}

}

