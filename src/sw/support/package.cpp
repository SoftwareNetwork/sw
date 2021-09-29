// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>

#include "package.h"

#include "storage.h"

#include <fmt/ostream.h>

//#include <primitives/log.h>
//DECLARE_STATIC_LOGGER(logger, "package");

namespace sw
{

String getSourceDirectoryName()
{
    // we cannot change it, because server already has such packages
    // introduce versions to change this or smth
    return "sdir";
}

static path getHashPathFromHash(const String &h, int nsubdirs, int chars_per_subdir)
{
    path p;
    int i = 0;
    for (; i < nsubdirs; i++)
        p /= h.substr(i * chars_per_subdir, chars_per_subdir);
    p /= h.substr(i * chars_per_subdir);
    return p;
}

String PackageData::getHash(StorageFileType type, size_t config_hash) const
{
    if (type == StorageFileType::SourceArchive)
        return hash;
    throw SW_RUNTIME_ERROR("Cannot return other hashes");
}

Package::Package(const IStorage &storage, const PackageId &id)
    : PackageId(id), storage(storage)
{
}

Package::Package(const Package &rhs)
    : PackageId(rhs), storage(rhs.storage), data(rhs.data ? rhs.data->clone() : nullptr)
{
}

String Package::getHash() const
{
    // move these calculations to storage?
    switch (storage.getSchema().getHashVersion())
    {
    case 1:
        return blake2b_512(getPath().toStringLower() + "-" + getVersion().toString());
    }

    throw SW_RUNTIME_ERROR("Unknown hash schema version: " + std::to_string(storage.getSchema().getHashVersion()));
}

path Package::getHashPath() const
{
    // move these calculations to storage?
    switch (storage.getSchema().getHashPathFromHashVersion())
    {
    case 1:
        return ::sw::getHashPathFromHash(getHash(), 4, 2); // remote consistent storage paths
    case 2:
        return ::sw::getHashPathFromHash(getHashShort(), 2, 2); // local storage is more relaxed
    }

    throw SW_RUNTIME_ERROR("Unknown hash path schema version: " + std::to_string(storage.getSchema().getHashPathFromHashVersion()));
}

String Package::getHashShort() const
{
    return shorten_hash(getHash(), 8);
}

String Package::formatPath(const String &s) const
{
    // {PHPF} = package hash path full
    // {PH64} = package hash, length = 64
    // {FN} = archive name
    return fmt::format(fmt::runtime(s),
        fmt::arg("PHPF", to_string(normalize_path(getHashPath()))),
        fmt::arg("PH64", getHash().substr(0, 64)),
        fmt::arg("FN", support::make_archive_name())
    );
}

const PackageData &Package::getData() const
{
    if (!data)
        data = storage.loadData(*this);
    return *data;
}

const IStorage &Package::getStorage() const
{
    return storage;
}

}
