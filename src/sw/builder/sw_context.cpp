// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin

#include "sw_context.h"

#include "command_storage.h"
#include "file_storage.h"

#include <boost/thread/lock_types.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <primitives/executor.h>

#include <regex>

//#include <primitives/log.h>
//DECLARE_STATIC_LOGGER(logger, "builder.context");

namespace sw
{

SwBuilderContext::SwBuilderContext()
{
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

CommandStorage &SwBuilderContext::getCommandStorage(const path &root) const
{
    std::unique_lock lk(csm);
    auto &cs = command_storages[root];
    if (!cs)
        cs = std::make_unique<CommandStorage>(root);
    return *cs;
}

void SwBuilderContext::clearFileStorages()
{
    file_storage.reset();
}

void SwBuilderContext::clearCommandStorages()
{
    std::unique_lock lk(csm);
    command_storages.clear();
}

}

