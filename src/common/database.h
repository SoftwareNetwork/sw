/*
 * Copyright (C) 2016-2017, Egor Pugin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "cppan_string.h"
#include "dependency.h"
#include "filesystem.h"

#include <primitives/date_time.h>

#include <chrono>
#include <memory>
#include <vector>

class SqliteDatabase;
struct Package;

struct TableDescriptor
{
    String name;
    String query;
};

using TableDescriptors = const std::vector<TableDescriptor>;

struct StartupAction
{
    enum Type
    {
        // append only
        ClearCache                  = 0,
        ServiceDbClearConfigHashes  = (1 << 0),
        CheckSchema                 = (1 << 1),
        ClearStorageDirExp          = (1 << 2),
        ClearSourceGroups           = (1 << 3),
        ClearStorageDirBin          = (1 << 4),
        ClearStorageDirLib          = (1 << 5),
        ClearCfgDirs                = (1 << 6),
        ClearPackagesDatabase       = (1 << 7),
        ClearStorageDirObj          = (1 << 8),
    };

    int id;
    int action;
};

class Database
{
public:
    Database(const String &name, const TableDescriptors &tds);
    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    void open(bool read_only = false);

protected:
    std::unique_ptr<SqliteDatabase> db;
    path fn;
    path db_dir;
    bool created = false;
    const TableDescriptors &tds;

    void recreate();
};

class ServiceDatabase : public Database
{
public:
    ServiceDatabase();

    void init();

    void performStartupActions() const;

    void checkForUpdates() const;
    TimePoint getLastClientUpdateCheck() const;
    void setLastClientUpdateCheck(const TimePoint &p = Clock::now()) const;

    int getNumberOfRuns() const;
    int increaseNumberOfRuns() const; // returns previous value

    int getPackagesDbSchemaVersion() const;
    void setPackagesDbSchemaVersion(int version) const;

    bool isActionPerformed(const StartupAction &action) const;
    void setActionPerformed(const StartupAction &action) const;

    String getConfigByHash(const String &settings_hash) const;
    void addConfigHash(const String &settings_hash, const String &config, const String &config_hash) const;
    void clearConfigHashes() const;
    void removeConfigHashes(const String &config_hash) const;

    void setPackageDependenciesHash(const Package &p, const String &hash) const;
    bool hasPackageDependenciesHash(const Package &p, const String &hash) const;

    void addInstalledPackage(const Package &p) const;
    void removeInstalledPackage(const Package &p) const;
    String getInstalledPackageHash(const Package &p) const;
    int getInstalledPackageId(const Package &p) const;
    PackagesSet getInstalledPackages() const;

    void setSourceGroups(const Package &p, const SourceGroups &sg) const;
    SourceGroups getSourceGroups(const Package &p) const;
    void removeSourceGroups(const Package &p) const;
    void removeSourceGroups(int id) const;
    void clearSourceGroups() const;

    Stamps getFileStamps() const;
    void setFileStamps(const Stamps &stamps) const;
    void clearFileStamps() const;

private:
    void createTables() const;
    void checkStamp() const;

    String getTableHash(const String &table) const;
    void setTableHash(const String &table, const String &hash) const;

    void recreateTable(const TableDescriptor &td) const;
};

class PackagesDatabase : public Database
{
    using Dependencies = DownloadDependency::DbDependencies;
    using DependenciesMap = std::unordered_map<Package, DownloadDependency>;

public:
    PackagesDatabase();

    IdDependencies findDependencies(const Packages &deps) const;

    void listPackages(const String &name = String()) const;

    template <template <class...> class C>
    C<ProjectPath> getMatchingPackages(const String &name = String()) const;
    std::vector<Version> getVersionsForPackage(const ProjectPath &ppath) const;
    Version getExactVersionForPackage(const Package &p) const;

    PackagesSet getDependentPackages(const Package &pkg);
    PackagesSet getDependentPackages(const PackagesSet &pkgs);
    PackagesSet getTransitiveDependentPackages(const PackagesSet &pkgs);

    ProjectId getPackageId(const ProjectPath &ppath) const;

private:
    path db_repo_dir;

    void init();
    void download();
    void load(bool drop = false);

    void writeDownloadTime() const;
    TimePoint readDownloadTime() const;

    bool isCurrentDbOld() const;

    ProjectVersionId getExactProjectVersionId(const DownloadDependency &project, Version &version, ProjectFlags &flags, String &hash) const;
    Dependencies getProjectDependencies(ProjectVersionId project_version_id, DependenciesMap &dm) const;
};

ServiceDatabase &getServiceDatabase(bool init = true);
ServiceDatabase &getServiceDatabaseReadOnly();
PackagesDatabase &getPackagesDatabase();

int readPackagesDbSchemaVersion(const path &dir);
void writePackagesDbSchemaVersion(const path &dir);

int readPackagesDbVersion(const path &dir);
void writePackagesDbVersion(const path &dir, int version);
