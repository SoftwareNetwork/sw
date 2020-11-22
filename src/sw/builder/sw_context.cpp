/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2020 Egor Pugin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

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
    file_storage_executor = std::make_unique<Executor>("async log writer", 1);
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
        cs = std::make_unique<CommandStorage>(*this, root);
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

