// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

//#include "dependency.h"
#include "package.h"

#include <primitives/filesystem.h>

#include <optional>

namespace sw
{

struct ISwContext;

//using ResolvedPackagesMap = std::unordered_map<UnresolvedPackage, DownloadDependency>;

struct SW_MANAGER_API PackageStore
{
    //using Dependencies = std::unordered_set<DownloadDependency>;

    ISwContext &swctx;
    //ResolvedPackagesMap resolved_packages;

    PackageStore(ISwContext &swctx);

    //std::optional<ExtendedPackageData> isPackageResolved(const UnresolvedPackage &);

    void loadLockFile(const path &fn);
    void saveLockFile(const path &fn) const;
    bool canUseLockFile() const;

private:
    bool use_lock_file = false;
    //Dependencies download_dependencies_;

    bool processing = false;
    bool deps_changed = false;

    friend class Resolver;
};

struct SW_MANAGER_API Resolver
{
    //using Dependencies = std::unordered_set<DownloadDependency>;

    //ResolvedPackagesMap resolved_packages;
    bool add_downloads = true;

    Resolver() = default;
    Resolver(const Resolver &) = delete;

    //void resolve(const UnresolvedPackages &pkgs);

    //std::unordered_set<ExtendedPackageData> getDownloadDependencies() const;

private:
    //Dependencies download_dependencies_;
    //const Remote *current_remote = nullptr;
    //bool query_local_db = true;

    void download_and_unpack();

    void resolve(const UnresolvedPackages &deps, std::function<void()> resolve_action);
    void resolve1(const UnresolvedPackages &deps, std::function<void()> resolve_action);
    //void download(const ExtendedPackageData &d, const path &fn);
    void markAsResolved(const UnresolvedPackages &deps);
    //static void add_dep(Dependencies &dd, const PackageId &d);
};

SW_MANAGER_API
Packages resolve_dependency(const String &d);

//SW_MANAGER_API
//ResolvedPackagesMap resolve_dependencies(const UnresolvedPackages &deps);

}
