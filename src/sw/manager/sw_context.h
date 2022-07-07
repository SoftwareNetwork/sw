// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "package.h"

#include <sw/support/filesystem.h>

#include <primitives/executor.h>

#include <memory>
#include <mutex>
#include <vector>

namespace sw
{

struct IResolvableStorage;
struct CachedStorage;
struct LocalStorage;
struct ResolveResultWithDependencies;

// sw_context_t?
/*struct SW_MANAGER_API ISwContext
{
    virtual ~ISwContext() = 0;

    virtual Package resolve(const UnresolvedPackage &) const = 0;
};*/

struct SW_MANAGER_API SwManagerContext// : ISwContext
{
    std::unique_ptr<Executor> executor;

    SwManagerContext(const path &local_storage_root_dir, bool allow_network);
    virtual ~SwManagerContext();

    LocalStorage &getLocalStorage();
    const LocalStorage &getLocalStorage() const;
    std::vector<IStorage *> getRemoteStorages() const;

    //
    std::unordered_map<UnresolvedPackage, LocalPackage> install(const UnresolvedPackages &, bool use_cache = true) const;
    LocalPackage install(const Package &) const;

    ResolveResultWithDependencies resolve(const UnresolvedPackages &, bool use_cache = true) const;
    LocalPackage resolve(const UnresolvedPackage &) const;
    ResolveResultWithDependencies resolve(const UnresolvedPackages &, const std::vector<IStorage*> &) const;

    // lock file related
    void setCachedPackages(const std::unordered_map<UnresolvedPackage, PackageId> &) const;

private:
    int cache_storage_id;
    int local_storage_id;
    int first_remote_storage_id;
    std::vector<std::unique_ptr<IStorage>> storages;
    mutable std::mutex resolve_mutex;

    CachedStorage &getCachedStorage() const;
};

} // namespace sw
