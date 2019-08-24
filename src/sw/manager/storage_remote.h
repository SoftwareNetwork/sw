// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "storage.h"

namespace sw
{

// main/web/url etc. storage
struct SW_MANAGER_API RemoteStorage : StorageWithPackagesDatabase
{
    // also pass url, etc.
    // maybe pass root_db_dir / name directly
    RemoteStorage(LocalStorage &, const Remote &);
    virtual ~RemoteStorage();

    const StorageSchema &getSchema() const override { return schema; }
    //LocalPackage download(const PackageId &) const override;
    //LocalPackage install(const Package &) const;
    std::unique_ptr<vfs::File> getFile(const PackageId &id, StorageFileType) const override;
    std::unordered_map<UnresolvedPackage, PackagePtr> resolve(const UnresolvedPackages &pkgs, UnresolvedPackages &unresolved_pkgs) const override;

    const Remote &getRemote() const { return r; }

private:
    const Remote &r;
    LocalStorage &ls;
    SoftwareNetworkStorageSchema schema;
    path db_repo_dir;

    void download() const;
    void load() const;
    void updateDb() const;
    void preInitFindDependencies() const;
    void writeDownloadTime() const;
    TimePoint readDownloadTime() const;
    bool isCurrentDbOld() const;
};

struct SW_MANAGER_API RemoteStorageWithFallbackToRemoteResolving : RemoteStorage
{
    RemoteStorageWithFallbackToRemoteResolving(LocalStorage &, const Remote &);

    PackageDataPtr loadData(const PackageId &) const override;
    std::unordered_map<UnresolvedPackage, PackagePtr> resolveFromRemote(const UnresolvedPackages &pkgs, UnresolvedPackages &unresolved_pkgs) const;
    std::unordered_map<UnresolvedPackage, PackagePtr> resolve(const UnresolvedPackages &pkgs, UnresolvedPackages &unresolved_pkgs) const override;

private:
    mutable std::unordered_map<PackageId, PackageData> data;
};

SW_MANAGER_API
int readPackagesDbSchemaVersion(const path &dir);

SW_MANAGER_API
void writePackagesDbSchemaVersion(const path &dir);

SW_MANAGER_API
int readPackagesDbVersion(const path &dir);

SW_MANAGER_API
void writePackagesDbVersion(const path &dir, int version);

} // namespace sw
