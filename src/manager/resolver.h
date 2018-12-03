// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "dependency.h"

#include <primitives/stdcompat/optional.h>

namespace sw
{

using ResolvedPackagesMap = std::unordered_map<UnresolvedPackage, DownloadDependency>;

struct SW_MANAGER_API PackageStore
{
    using Dependencies = std::unordered_set<DownloadDependency>;

    ResolvedPackagesMap resolved_packages;

    void clear();

    optional<ExtendedPackageData> isPackageResolved(const UnresolvedPackage &);

    void loadLockFile(const path &fn);
    void saveLockFile(const path &fn) const;

private:
    bool use_lock_file = false;
    Dependencies download_dependencies_;

    bool processing = false;
    bool deps_changed = false;

    friend class Resolver;
};

class SW_MANAGER_API Resolver
{
public:
    using Dependencies = std::unordered_set<DownloadDependency>;

public:
    ResolvedPackagesMap resolved_packages;

    Resolver() = default;
    Resolver(const Resolver &) = delete;

    void resolve_dependencies(const UnresolvedPackages &deps, bool clean_resolve = false);
    void resolve_and_download(const UnresolvedPackage &p, const path &fn);

    std::unordered_set<ExtendedPackageData> getDownloadDependencies() const;
    std::unordered_map<ExtendedPackageData, PackageVersionGroupNumber> getDownloadDependenciesWithGroupNumbers() const;

private:
    Dependencies download_dependencies_;
    const Remote *current_remote = nullptr;
    bool query_local_db = true;

    void download_and_unpack();

    void resolve(const UnresolvedPackages &deps, std::function<void()> resolve_action);
    void resolve1(const UnresolvedPackages &deps, std::function<void()> resolve_action);
    void download(const ExtendedPackageData &d, const path &fn);
    static void add_dep(Dependencies &dd, const PackageId &d);
};

SW_MANAGER_API
void resolve_and_download(const Package &p, const path &fn);

SW_MANAGER_API
Packages resolve_dependency(const String &d);

SW_MANAGER_API
ResolvedPackagesMap resolve_dependencies(const UnresolvedPackages &deps);

SW_MANAGER_API
std::unordered_set<ExtendedPackageData> resolveAllDependencies(const UnresolvedPackages &deps);

SW_MANAGER_API
PackageStore &getPackageStore();

}
