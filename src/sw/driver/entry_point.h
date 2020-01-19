// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "build_settings.h"
#include "module.h"

namespace sw
{

struct NativeTargetEntryPoint;
struct Target;
struct SharedLibraryTarget;
struct Build;
struct Checker;
struct Module;

// this driver ep
struct NativeTargetEntryPoint : TargetEntryPoint
{
    path source_dir;

    [[nodiscard]]
    std::vector<ITargetPtr> loadPackages(SwBuild &, const TargetSettings &, const PackageIdSet &pkgs, const PackagePath &prefix) const override;

private:
    virtual void loadPackages1(Build &) const = 0;
};

struct PrepareConfigEntryPoint : NativeTargetEntryPoint
{
    mutable path out;
    mutable FilesMap r;
    mutable std::unique_ptr<PackageId> tgt;

    PrepareConfigEntryPoint(const std::unordered_set<LocalPackage> &pkgs);
    PrepareConfigEntryPoint(const Files &files);

    bool isOutdated() const;

private:
    const std::unordered_set<LocalPackage> pkgs_;
    mutable Files files_;
    mutable FilesSorted pkg_files_;

    void loadPackages1(Build &) const override;

    SharedLibraryTarget &createTarget(Build &, const String &name) const;
    decltype(auto) commonActions(Build &, const FilesSorted &files, const UnresolvedPackages &deps) const;
    void commonActions2(Build &, SharedLibraryTarget &lib) const;

    // many input files to many dlls
    void many2many(Build &, const Files &files) const;

    // many input files into one dll
    void many2one(Build &, const std::unordered_set<LocalPackage> &pkgs) const;

    // one input file to one dll
    void one2one(Build &, const path &fn) const;
};

struct NativeBuiltinTargetEntryPoint : NativeTargetEntryPoint
{
    using BuildFunction = std::function<void(Build &)>;
    using CheckFunction = std::function<void(Checker &)>;

    BuildFunction bf;
    CheckFunction cf;

    NativeBuiltinTargetEntryPoint(BuildFunction bf);

private:
    void loadPackages1(Build &) const override;
};

struct NativeModuleTargetEntryPoint : NativeTargetEntryPoint
{
    NativeModuleTargetEntryPoint(const Module &m);

private:
    Module m;

    void loadPackages1(Build &) const override;
};

}
