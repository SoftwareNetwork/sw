// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "dependency.h"
#include "filesystem.h"
#include "remote.h"

#include <primitives/date_time.h>
#include <optional>
#include <sqlpp11/sqlite3/connection.h>

#include <chrono>
#include <memory>
#include <vector>

namespace sw
{

class SqliteDatabase;
struct PackageId;

struct StartupAction
{
    enum Type
    {
        // append only
        ClearCache = 0x0000,
        ServiceDbClearConfigHashes = 0x0001,
        //CheckSchema                 = 0x0002,
        ClearStorageDirExp = 0x0004,
        //ClearSourceGroups           = 0x0008,
        ClearStorageDirBin = 0x0010,
        ClearStorageDirLib = 0x0020,
        ClearCfgDirs = 0x0040,
        ClearPackagesDatabase = 0x0080,
    };

    int id;
    int action;
};

class SW_MANAGER_API Database
{
public:
    Database(const String &name, const String &schema);
    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    void open(bool read_only = false);

protected:
    std::unique_ptr<sqlpp::sqlite3::connection> db;
    path fn;
    path db_dir;
    bool created = false;

    void recreate();

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
    using OverriddenPackages = PackageVersionMapBase<OverriddenPackage, std::unordered_map, std::map>;

    ServiceDatabase();

    void init();

    void performStartupActions() const;

    void checkForUpdates() const;
    TimePoint getLastClientUpdateCheck() const;
    void setLastClientUpdateCheck(const TimePoint &p = Clock::now()) const;

    int getPackagesDbSchemaVersion() const;
    void setPackagesDbSchemaVersion(int version) const;

    bool isActionPerformed(const StartupAction &action) const;
    void setActionPerformed(const StartupAction &action) const;

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

class SW_MANAGER_API PackagesDatabase : public Database
{
    using Dependencies = DownloadDependency::DbDependencies;
    //using Dependencies = DownloadDependency::IdDependenciesSet; // see dependency.h note
    using DependenciesMap = std::map<PackageId, DownloadDependency>;

public:
    PackagesDatabase();

    IdDependencies findDependencies(const UnresolvedPackages &deps) const;
    void findLocalDependencies(IdDependencies &id_deps, const UnresolvedPackages &deps) const;

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

    DataSources getDataSources();

private:
    path db_repo_dir;

    void download() const;
    void load(bool drop = false) const;

    void writeDownloadTime() const;
    TimePoint readDownloadTime() const;

    bool isCurrentDbOld() const;
    void updateDb() const;

    void preInitFindDependencies() const;
    db::PackageVersionId getExactProjectVersionId(const DownloadDependency &project, Version &version, SomeFlags &flags, String &hash, PackageVersionGroupNumber &gn, int &prefix) const;
    Dependencies getProjectDependencies(db::PackageVersionId project_version_id, DependenciesMap &dm) const;
};

SW_MANAGER_API
ServiceDatabase &getServiceDatabase(bool init = true);

SW_MANAGER_API
ServiceDatabase &getServiceDatabaseReadOnly();

SW_MANAGER_API
PackagesDatabase &getPackagesDatabase();

SW_MANAGER_API
int readPackagesDbSchemaVersion(const path &dir);

SW_MANAGER_API
void writePackagesDbSchemaVersion(const path &dir);

SW_MANAGER_API
int readPackagesDbVersion(const path &dir);

SW_MANAGER_API
void writePackagesDbVersion(const path &dir, int version);

}
