// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "package_version_map.h"
#include "remote.h"

#include <sw/support/filesystem.h>

#include <primitives/date_time.h>
#include <optional>

#include <chrono>
#include <memory>
#include <mutex>
#include <vector>

namespace sqlpp::sqlite3 { class connection; }

namespace sw
{

struct LocalStorage;
struct PackageId;

struct SW_MANAGER_API Database
{
    std::unique_ptr<sqlpp::sqlite3::connection> db;
    path fn;
    path db_dir;

    Database(const path &db_dir, const String &name, const String &schema);
    ~Database();

    void open(bool read_only = false);

    int getIntValue(const String &key);
    void setIntValue(const String &key, int v);

protected:
    //
    template <typename T>
    std::optional<T> getValue(const String &key) const;

    template <typename T>
    T getValue(const String &key, const T &default_) const;

    template <typename T>
    void setValue(const String &key, const T &v) const;
};

struct SW_MANAGER_API ServiceDatabase : public Database
{
    struct OverriddenPackage
    {
        path sdir;
        UnresolvedPackages deps;

        // extended
        db::PackageVersionId id = 0; // overridden id is less than 0
        int prefix = 2;

        int64_t getGroupNumber() const
        {
            auto gn = std::hash<path>()(sdir);
            if (gn > 0)
                gn = -gn;
            return gn;
        }
    };
    using OverriddenPackages = PackageVersionMapBase<OverriddenPackage, std::unordered_map, primitives::version::VersionMap>;

    ServiceDatabase(const path &db_dir);

    void init();

    void checkForUpdates() const;
    TimePoint getLastClientUpdateCheck() const;
    void setLastClientUpdateCheck(const TimePoint &p = Clock::now()) const;

    int getPackagesDbSchemaVersion() const;
    void setPackagesDbSchemaVersion(int version) const;

    String getConfigByHash(const String &settings_hash) const;
    int addConfig(const String &config) const;
    int getConfig(const String &config) const;
    void addConfigHash(const String &settings_hash, const String &config, const String &config_hash) const;
    void clearConfigHashes() const;
    void removeConfigHashes(const String &config_hash) const;

    void setPackageDependenciesHash(const PackageId &p, const String &hash) const;
    bool hasPackageDependenciesHash(const PackageId &p, const String &hash) const;

    void addInstalledPackage(const PackageId &p, PackageVersionGroupNumber group_number) const;
    void removeInstalledPackage(const PackageId &p) const;
    String getInstalledPackageHash(const PackageId &p) const;
    int64_t getInstalledPackageId(const PackageId &p) const;
    int getInstalledPackageConfigId(const PackageId &p, const String &config) const;
    SomeFlags getInstalledPackageFlags(const PackageId &p, const String &config) const;
    void setInstalledPackageFlags(const PackageId &p, const String &config, const SomeFlags &f) const;
    bool isPackageInstalled(const PackageId &p) const { return getInstalledPackageId(p) != 0; }
    Packages getInstalledPackages() const;

    std::optional<OverriddenPackage> getOverriddenPackage(const PackageId &pkg) const;
    const OverriddenPackages &getOverriddenPackages() const;
    void overridePackage(const PackageId &pkg, const OverriddenPackage &opkg) const;
    void deleteOverriddenPackage(const PackageId &pkg) const;
    void deleteOverriddenPackageDir(const path &sdir) const;
    UnresolvedPackages getOverriddenPackageVersionDependencies(db::PackageVersionId project_version_id);

private:
    mutable std::optional<OverriddenPackages> override_remote_packages;
};

struct SW_MANAGER_API PackagesDatabase : public Database
{
    //using Dependencies = DownloadDependency::DbDependencies;
    //using Dependencies = DownloadDependency::IdDependenciesSet; // see dependency.h note
    //using DependenciesMap = std::map<PackageId, DownloadDependency>;

    PackagesDatabase(const path &db_fn);

    std::unordered_map<UnresolvedPackage, PackageId> resolve(const UnresolvedPackages &pkgs, UnresolvedPackages &unresolved_pkgs) const;
    PackageData getPackageData(const PackageId &) const;

    int64_t getInstalledPackageId(const PackageId &p) const;
    String getInstalledPackageHash(const PackageId &p) const;
    bool isPackageInstalled(const Package &p) const;
    void installPackage(const Package &p);
    DataSources getDataSources() const;
    std::optional<path> getOverriddenDir(const Package &p) const;

    //IdDependencies findDependencies(const UnresolvedPackages &deps) const;
    //void findLocalDependencies(IdDependencies &id_deps, const UnresolvedPackages &deps) const;

    void listPackages(const String &name = String()) const;

    template <template <class...> class C>
    C<PackagePath> getMatchingPackages(const String &name = String()) const;
    std::vector<Version> getVersionsForPackage(const PackagePath &ppath) const;
    Version getExactVersionForPackage(const PackageId &p) const;

    Packages getDependentPackages(const PackageId &pkg);
    Packages getDependentPackages(const Packages &pkgs);
    Packages getTransitiveDependentPackages(const Packages &pkgs);

    db::PackageId getPackageId(const PackagePath &ppath) const;
    PackageId getGroupLeader(PackageVersionGroupNumber) const;

    //std::optional<ExtendedPackageData> getPackageInformation(const PackageId &) const;

private:
    std::mutex m;

    PackageVersionGroupNumber getMaxGroupNumber() const;

    //db::PackageVersionId getExactProjectVersionId(const DownloadDependency &project, Version &version, SomeFlags &flags, String &hash, PackageVersionGroupNumber &gn, int &prefix) const;
    //Dependencies getProjectDependencies(db::PackageVersionId project_version_id, DependenciesMap &dm) const;
};

}
