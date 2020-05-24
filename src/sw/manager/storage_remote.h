// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "storage.h"

#include <primitives/date_time.h>

namespace sw
{

// main/web/url etc. storage
struct SW_MANAGER_API RemoteStorage : StorageWithPackagesDatabase
{
    // also pass url, etc.
    // maybe pass root_db_dir / name directly
    RemoteStorage(LocalStorage &, const Remote &, bool allow_network);
    virtual ~RemoteStorage();

    const StorageSchema &getSchema() const override { return schema; }
    //LocalPackage download(const PackageId &) const override;
    //LocalPackage install(const Package &) const;
    std::unique_ptr<vfs::File> getFile(const PackageId &id, StorageFileType) const override;
    std::unordered_map<UnresolvedPackage, PackagePtr> resolve(const UnresolvedPackages &pkgs, UnresolvedPackages &unresolved_pkgs) const override;

    const Remote &getRemote() const { return r; }

    bool isNetworkAllowed() const { return allow_network; }

private:
    const Remote &r;
    LocalStorage &ls;
    SoftwareNetworkStorageSchema schema;
    path db_repo_dir;
    bool allow_network;

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
    RemoteStorageWithFallbackToRemoteResolving(LocalStorage &, const Remote &, bool allow_network);

    PackageDataPtr loadData(const PackageId &) const override;
    std::unordered_map<UnresolvedPackage, PackagePtr> resolveFromRemote(const UnresolvedPackages &pkgs, UnresolvedPackages &unresolved_pkgs) const;
    std::unordered_map<UnresolvedPackage, PackagePtr> resolve(const UnresolvedPackages &pkgs, UnresolvedPackages &unresolved_pkgs) const override;

private:
    mutable std::unordered_map<PackageId, PackageData> data;
    mutable bool remote_resolving_is_not_working = false;
};

} // namespace sw
