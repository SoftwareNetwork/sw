// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2016-2019 Egor Pugin

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
    LocalPackage &operator=(LocalPackage &&) = delete;
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
