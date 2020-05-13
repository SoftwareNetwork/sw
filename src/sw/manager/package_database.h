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

#include "database.h"
#include "package.h"

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

    db::PackageVersionId getInstalledPackageId(const PackageId &) const;
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
    db::PackageVersionId getPackageVersionId(const PackageId &) const;
    String getPackagePath(db::PackageId) const;

    std::vector<PackagePath> getMatchingPackages(const String &name = {}, int limit = 0, int offset = 0) const;
    std::vector<Version> getVersionsForPackage(const PackagePath &) const;

private:
    std::mutex m;
    std::unique_ptr<struct PreparedStatements> pps;

    // add type and config later
    // rename to get package version file hash ()
    String getInstalledPackageHash(db::PackageVersionId) const;
};

}
