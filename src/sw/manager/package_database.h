// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "database.h"
#include "package.h"
#include "remote.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace sw
{

struct LocalStorage;
struct PackageId;

struct SW_MANAGER_API PackagesDatabase : Database
{
    PackagesDatabase(const path &db_fn);
    ~PackagesDatabase();

    void open(bool read_only = false, bool in_memory = false);

    std::unordered_map<UnresolvedPackage, PackageId> resolve(const UnresolvedPackages &pkgs, UnresolvedPackages &unresolved_pkgs) const;

    PackageData getPackageData(const PackageId &) const;

    int64_t getInstalledPackageId(const PackageId &) const;
    String getInstalledPackageHash(const PackageId &) const;
    bool isPackageInstalled(const Package &) const;
    void installPackage(const Package &);
    void installPackage(const PackageId &, const PackageData &);
    void deletePackage(const PackageId &) const;

    // overridden
    std::optional<path> getOverriddenDir(const Package &p) const;
    std::unordered_set<PackageId> getOverriddenPackages() const;
    void deleteOverriddenPackageDir(const path &sdir) const;

    DataSources getDataSources() const;

    db::PackageId getPackageId(const PackagePath &) const;
    db::PackageId getPackageVersionId(const PackageId &) const;
    String getPackagePath(db::PackageId) const;

    std::vector<PackagePath> getMatchingPackages(const String &name = {}, int limit = 0, int offset = 0) const;
    std::vector<Version> getVersionsForPackage(const PackagePath &) const;

private:
    std::mutex m;
    std::unique_ptr<struct PreparedStatements> pps;
};

}
