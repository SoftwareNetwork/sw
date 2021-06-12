// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <sw/support/enums.h>
#include <sw/support/package.h>

namespace sw
{

struct IStorage;
struct LocalStorage;
struct OverriddenPackagesStorage;

struct SW_MANAGER_API LocalPackageBase : Package
{
    using Package::Package;

    virtual bool isOverridden() const { return false; }
};

struct SW_MANAGER_API LocalPackage : LocalPackageBase
{
    LocalPackage(const LocalStorage &, const PackageId &);

    virtual std::unique_ptr<Package> clone() const { return std::make_unique<LocalPackage>(*this); }

    /// main package dir
    path getDir() const;

    /// source archive root
    path getDirSrc() const;
    /// actual sources root
    virtual path getSourceDirectory() const;

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

struct SW_MANAGER_API OverriddenPackage : LocalPackageBase
{
    using LocalPackageBase::LocalPackageBase;

    path getSourceDirectory() const override;
    bool isOverridden() const override { return true; }
    std::unique_ptr<Package> clone() const override { return std::make_unique<OverriddenPackage>(*this); }
};

//using LocalPackagePtr = std::unique_ptr<LocalPackage>;

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
