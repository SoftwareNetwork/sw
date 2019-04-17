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

struct LocalStorage;
struct PackageStore;  // remove when vs bug is fixed
struct Storage;

// sw_context_t
struct ISwContext
{
    virtual ~ISwContext() = default;

    virtual LocalStorage &getLocalStorage() = 0;
    virtual const LocalStorage &getLocalStorage() const = 0;

    // rename to resolvePackages?
    virtual std::unordered_map<UnresolvedPackage, Package> resolve(const UnresolvedPackages &) const = 0;
};

struct SW_MANAGER_API SwManagerContext : ISwContext
{
    std::unique_ptr<PackageStore> store; // remove when vs bug is fixed
    std::vector<std::unique_ptr<Storage>> storages;

    SwManagerContext(const path &local_storage_root_dir);
    virtual ~SwManagerContext();

    LocalStorage &getLocalStorage() override;
    const LocalStorage &getLocalStorage() const override;

    std::unordered_map<UnresolvedPackage, Package> resolve(const UnresolvedPackages &) const override;

    std::unordered_map<UnresolvedPackage, Package> resolve(const UnresolvedPackages &unresolved, std::unordered_map<UnresolvedPackage, Package> &resolved_packages) const;
    bool isResolved(const UnresolvedPackage &pkg) const;
    LocalPackage resolve(const UnresolvedPackage &) const;

protected:
    mutable std::mutex m; // main context mutex

private:
    mutable std::unordered_map<UnresolvedPackage, Package> resolved_packages;
};

} // namespace sw
