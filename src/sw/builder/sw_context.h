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

#pragma once

#include "concurrent_map.h"
#include "module_storage.h"
#include "os.h"

#include <sw/manager/sw_context.h>

#include <shared_mutex>

struct Executor;

namespace sw
{

struct CommandStorage;
struct FileStorage;

namespace builder::detail { struct ResolvableCommand; }

struct SW_BUILDER_API SwBuilderContext : SwManagerContext
{
    OS HostOS;

    SwBuilderContext(const path &local_storage_root_dir);
    virtual ~SwBuilderContext();

    FileStorage &getFileStorage() const;
    Executor &getFileStorageExecutor() const;
    CommandStorage &getCommandStorage(const path &root) const;
    ModuleStorage &getModuleStorage() const;
    const OS &getHostOs() const { return HostOS; }

    void clearFileStorages();

private:
    std::unique_ptr<ModuleStorage> module_storage;
    // keep order
    mutable std::unordered_map<path, std::unique_ptr<CommandStorage>> command_storages;
    mutable std::unique_ptr<FileStorage> file_storage;
    std::unique_ptr<Executor> file_storage_executor; // after everything!

    mutable std::mutex csm;
};

} // namespace sw
