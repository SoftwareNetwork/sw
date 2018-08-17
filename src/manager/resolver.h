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

using ResolvedPackagesMap = std::unordered_map<UnresolvedPackage, ExtendedPackageData>;

class SW_MANAGER_API PackageStore
{
public:
    struct PackageConfig
    {
        // resolved?
        Packages dependencies;
    };
    using PackageConfigs = std::map<Package, PackageConfig>;

    using iterator = PackageConfigs::iterator;
    using const_iterator = PackageConfigs::const_iterator;

public:
    bool has_local_package(const PackagePath &ppath) const;
    path get_local_package_dir(const PackagePath &ppath) const;

public:
    PackageConfig & operator[](const Package &p);
    const PackageConfig &operator[](const Package &p) const;

    iterator begin();
    iterator end();

    const_iterator begin() const;
    const_iterator end() const;

    iterator find(const PackageConfigs::key_type &k) { return packages.find(k); }
    const_iterator find(const PackageConfigs::key_type &k) const { return packages.find(k); }

    bool empty() const { return packages.empty(); }
    size_t size() const { return packages.size(); }

    optional<ExtendedPackageData> isPackageResolved(const UnresolvedPackage &);

private:
    PackageConfigs packages;

    ResolvedPackagesMap resolved_packages;
    std::map<PackagePath, path> local_packages;

    bool processing = false;
    bool deps_changed = false;

    friend class Resolver;
};

class SW_MANAGER_API Resolver
{
public:
    using Dependencies = DownloadDependency::Dependencies;

public:
    ResolvedPackagesMap resolved_packages;

    Resolver() = default;
    Resolver(const Resolver &) = delete;

    void resolve_dependencies(const UnresolvedPackages &deps);
    void resolve_and_download(const UnresolvedPackage &p, const path &fn);

    std::unordered_set<ExtendedPackageData> getDownloadDependencies() const;
    std::unordered_map<ExtendedPackageData, PackageVersionGroupNumber> getDownloadDependenciesWithGroupNumbers() const;

private:
    Dependencies download_dependencies_;
    const Remote *current_remote = nullptr;
    bool query_local_db = true;

    void download_and_unpack();

    void resolve(const UnresolvedPackages &deps, std::function<void()> resolve_action);
    void download(const ExtendedPackageData &d, const path &fn);
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
