// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "enums.h"
#include "filesystem.h"

#include <primitives/sw/settings.h>

namespace sw
{

struct PackageId;

namespace storage
{

struct SW_MANAGER_API VirtualFileSystem
{
    virtual ~VirtualFileSystem() = default;

    virtual void writeFile(const PackageId &pkg, const path &local_file, const path &vfs_file) const = 0;
};

// default fs
struct SW_MANAGER_API LocalFileSystem : VirtualFileSystem
{

};

// more than one destination
struct SW_MANAGER_API VirtualFileSystemMultiplexer : VirtualFileSystem
{
    std::vector<std::shared_ptr<VirtualFileSystem>> filesystems;

    void writeFile(const PackageId &pkg, const path &local_file, const path &vfs_file) const override
    {
        for (auto &fs : filesystems)
            fs->writeFile(pkg, local_file, vfs_file);
    }
};

} // namespace storage

struct Storage
{
    path storage_dir;
#define DIR(x) path storage_dir_##x;
#include "storage_directories.inl"
#undef DIR
    path build_dir;

    SettingsType storage_dir_type;
    SettingsType build_dir_type;

    bool empty() const { return storage_dir.empty(); }
    void update(const Storage &s, SettingsType type);

    void set_storage_dir(const path &p);
    void set_build_dir(const path &p);

    path get_static_files_dir() const;

private:
    SettingsType type{ SettingsType::Max };
};

SW_MANAGER_API
const Storage &getStorage();

} // namespace sw
