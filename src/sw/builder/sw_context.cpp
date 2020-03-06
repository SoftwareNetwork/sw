// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "sw_context.h"

#include "command_storage.h"
#include "file_storage.h"

#include <sw/manager/storage.h>

#include <boost/thread/lock_types.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <primitives/executor.h>

#include <regex>

//#include <primitives/log.h>
//DECLARE_STATIC_LOGGER(logger, "builder.context");

namespace sw
{

SwBuilderContext::SwBuilderContext(const path &local_storage_root_dir)
    : SwManagerContext(local_storage_root_dir)
{
#ifdef _WIN32
    // with per pkg command log we must increase the limits
    //auto new_limit = 8192;
    //if (_setmaxstdio(new_limit) == -1)
        //LOG_ERROR(logger, "Cannot raise number of maximum opened files");
#endif

    HostOS = getHostOS();

    module_storage = std::make_unique<ModuleStorage>();

    //
    file_storage_executor = std::make_unique<Executor>("async log writer", 1);
}

SwBuilderContext::~SwBuilderContext()
{
    // do not clear modules on exception, because it may come from there
    // TODO: cleanup modules data first
    // copy exception here and pass further?
    //if (std::uncaught_exceptions())
        //module_storage.release();
}

ModuleStorage &SwBuilderContext::getModuleStorage() const
{
    return *module_storage;
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
        cs = std::make_unique<CommandStorage>(*this, root);
    return *cs;
}

void SwBuilderContext::clearFileStorages()
{
    file_storage.reset();
}

}

