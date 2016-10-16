/*
 * Copyright (c) 2016, Egor Pugin
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     1. Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *     3. Neither the name of the copyright holder nor the names of
 *        its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "common.h"
#include "dependency.h"

#include <chrono>
#include <memory>
#include <vector>

class SqliteDatabase;

struct TableDescriptor
{
    String name;
    String query;
};

using TableDescriptors = const std::vector<TableDescriptor>;

class Database
{
public:
    Database(const String &name, const TableDescriptors &tds);
    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

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

    int getNumberOfRuns() const;
    int increaseNumberOfRuns(); // returns previous value

    int getPackagesDbSchemaVersion() const;
    void setPackagesDbSchemaVersion(int version);
};

class PackagesDatabase : public Database
{
    using TimePoint = std::chrono::system_clock::time_point;

    using Dependencies = DownloadDependency::DbDependencies;
    using DependenciesMap = std::map<Package, DownloadDependency>;

public:
    PackagesDatabase();

    DownloadDependencies findDependencies(const Packages &deps) const;

    void listPackages(const String &name = String());

private:
    path db_repo_dir;

    void download();
    void load(bool drop = false);

    void writeDownloadTime() const;
    TimePoint readDownloadTime() const;

    bool isCurrentDbOld() const;

    ProjectVersionId getExactProjectVersionId(const DownloadDependency &project, Version &version, ProjectFlags &flags, String &sha256) const;
    Dependencies getProjectDependencies(ProjectVersionId project_version_id, DependenciesMap &dm) const;
};

ServiceDatabase &getServiceDatabase();
PackagesDatabase &getPackagesDatabase();

int readPackagesDbSchemaVersion(const path &dir);
void writePackagesDbSchemaVersion(const path &dir);

int readPackagesDbVersion(const path &dir);
void writePackagesDbVersion(const path &dir, int version);
