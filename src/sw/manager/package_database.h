// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "database.h"
#include "package.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace sw
{

struct IStorage;
struct PackageId;
struct ResolveRequest;

struct SW_MANAGER_API PackagesDatabase : Database
{
    PackagesDatabase(const path &db_fn);
    ~PackagesDatabase();

    void open(bool read_only = false, bool in_memory = false);

    bool resolve(ResolveRequest &, const IStorage &, bool allow_override) const;

    PackageData getPackageData(const PackageId &) const;

    String getInstalledPackageHash(const PackageId &) const;
    bool isPackageInstalled(const Package &) const;
    void installPackage(const Package &);
    void installPackage(const PackageId &, const PackageData &);
    void deletePackage(const PackageId &) const;

    // overridden
    std::optional<path> getOverriddenDir(const Package &p) const;
    std::unordered_set<PackageId> getOverriddenPackages() const;
    void deleteOverriddenPackageDir(const path &sdir) const;

    db::PackageId getPackageId(const PackagePath &) const;
    db::PackageVersionId getPackageVersionId(const PackageName &) const;
    String getPackagePath(db::PackageId) const;

    std::vector<PackagePath> getMatchingPackages(const String &name = {}, int limit = 0, int offset = 0) const;
    std::vector<PackageVersion> getVersionsForPackage(const PackagePath &) const;

private:
    std::mutex m;
    std::unique_ptr<struct PreparedStatements> pps;

    // add type and config later
    // rename to get package version file hash ()
    String getInstalledPackageHash(db::PackageVersionId) const;
};

}
