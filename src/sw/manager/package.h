// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <sw/support/enums.h>
#include <sw/support/package.h>

namespace sw
{

struct IStorage;
struct LocalStorage;
struct OverriddenPackagesStorage;

struct SW_MANAGER_API LocalPackage : Package
{
    LocalPackage(const LocalStorage &, const PackageId &);

    LocalPackage(const LocalPackage &) = default;
    LocalPackage &operator=(const LocalPackage &) = delete;
    LocalPackage(LocalPackage &&) = default;
    LocalPackage &operator=(LocalPackage &&) = default;
    virtual ~LocalPackage() = default;

    virtual std::unique_ptr<Package> clone() const { return std::make_unique<LocalPackage>(*this); }

    bool isOverridden() const;
    std::optional<path> getOverriddenDir() const;

    /// main package dir
    path getDir() const;

    /// source archive root
    path getDirSrc() const;
    /// actual sources root
    path getDirSrc2() const;

    //
    path getDirObj() const;
    path getDirObj(const String &cfg) const;

    //path getDirObjWdir() const;
    path getDirInfo() const;

    path getStampFilename() const;
    String getStampHash() const;

    void remove() const;

    const LocalStorage &getStorage() const;

private:
    path getDir(const path &root) const;
};

using LocalPackagePtr = std::unique_ptr<LocalPackage>;

SW_MANAGER_API
String getSourceDirectoryName();

}

namespace std
{

template<> struct hash<::sw::LocalPackage>
{
    size_t operator()(const ::sw::LocalPackage &p) const
    {
        return std::hash<::sw::Package>()(p);
    }
};

}
