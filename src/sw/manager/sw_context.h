/*
 * SW - Build System and Package Manager
 * Copyright (C) 2016-2019 Egor Pugin
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

#include "package.h"

#include <sw/support/filesystem.h>

#include <memory>
#include <mutex>
#include <vector>

namespace sw
{

struct IResolvableStorage;
struct Storage;
struct CachedStorage;
struct LocalStorage;

// sw_context_t?
/*struct SW_MANAGER_API ISwContext
{
    virtual ~ISwContext() = 0;

    virtual Package resolve(const UnresolvedPackage &) const = 0;
};*/

struct SW_MANAGER_API SwManagerContext// : ISwContext
{
    SwManagerContext(const path &local_storage_root_dir, bool allow_network);
    virtual ~SwManagerContext();

    LocalStorage &getLocalStorage();
    const LocalStorage &getLocalStorage() const;
    std::vector<Storage *> getRemoteStorages();
    std::vector<const Storage *> getRemoteStorages() const;

    //
    std::unordered_map<UnresolvedPackage, LocalPackage> install(const UnresolvedPackages &, bool use_cache = true) const;
    LocalPackage install(const Package &) const;

    std::unordered_map<UnresolvedPackage, PackagePtr> resolve(const UnresolvedPackages &, bool use_cache = true) const;
    LocalPackage resolve(const UnresolvedPackage &) const;

    // lock file related
    void setCachedPackages(const std::unordered_map<UnresolvedPackage, PackageId> &) const;

private:
    int cache_storage_id;
    int local_storage_id;
    int first_remote_storage_id;
    std::vector<std::unique_ptr<IStorage>> storages;
    mutable std::mutex resolve_mutex;

    CachedStorage &getCachedStorage() const;
    std::unordered_map<UnresolvedPackage, PackagePtr> resolve(const UnresolvedPackages &, const std::vector<IStorage*> &) const;
};

} // namespace sw
