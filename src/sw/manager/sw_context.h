// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
//struct SW_MANAGER_API ISwContext

struct SW_MANAGER_API SwManagerContext
{
    SwManagerContext(const path &local_storage_root_dir);
    virtual ~SwManagerContext();

    LocalStorage &getLocalStorage();
    const LocalStorage &getLocalStorage() const;
    std::vector<Storage *> getRemoteStorages();
    std::vector<const Storage *> getRemoteStorages() const;

    // move to builder?
    std::unordered_map<UnresolvedPackage, LocalPackage> install(const UnresolvedPackages &) const;
    LocalPackage install(const Package &) const;

    std::unordered_map<UnresolvedPackage, Package> resolve(const UnresolvedPackages &) const;
    LocalPackage resolve(const UnresolvedPackage &) const;

private:
    int cache_storage_id;
    int local_storage_id;
    int first_remote_storage_id;
    std::vector<std::unique_ptr<IResolvableStorage>> storages;
    mutable std::mutex resolve_mutex;

    //bool isResolved(const UnresolvedPackage &pkg) const;
    CachedStorage &getCachedStorage() const;
};

} // namespace sw
