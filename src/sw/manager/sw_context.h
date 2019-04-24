// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "storage.h"

#include <sw/support/filesystem.h>

#include <memory>
#include <mutex>
#include <vector>

namespace sw
{

// sw_context_t
struct ISwContext
{
    virtual ~ISwContext() = default;

    virtual Storage &getLocalStorage() = 0;
    virtual const Storage &getLocalStorage() const = 0;

    // rename to resolvePackages?
    virtual std::unordered_map<UnresolvedPackage, Package> resolve(const UnresolvedPackages &) const = 0;
};

struct SW_MANAGER_API SwManagerContext : ISwContext
{
    SwManagerContext(const path &local_storage_root_dir);
    virtual ~SwManagerContext();

    LocalStorage &getLocalStorage() override;
    const LocalStorage &getLocalStorage() const override;
    std::vector<Storage *> getRemoteStorages();
    std::vector<const Storage *> getRemoteStorages() const;

    std::unordered_map<UnresolvedPackage, Package> resolve(const UnresolvedPackages &) const override;

    // move to builder?
    std::unordered_map<UnresolvedPackage, LocalPackage> install(const UnresolvedPackages &) const;
    LocalPackage install(const Package &) const;

    std::unordered_map<UnresolvedPackage, Package> resolve(const UnresolvedPackages &unresolved, std::unordered_map<UnresolvedPackage, Package> &resolved_packages) const;
    bool isResolved(const UnresolvedPackage &pkg) const;
    LocalPackage resolve(const UnresolvedPackage &) const;

protected:
    mutable std::mutex m; // main context mutex

private:
    int local_storage_id;
    int first_remote_storage_id;
    std::vector<std::unique_ptr<Storage>> storages;
    mutable std::unordered_map<UnresolvedPackage, Package> resolved_packages;
};

} // namespace sw
