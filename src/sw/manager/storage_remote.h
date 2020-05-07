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
