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

struct ModuleSwappableDataBase
{
    PackagePath NamePrefix;
    PackageVersionGroupNumber current_gn = 0;
};

struct ModuleSwappableData : ModuleSwappableDataBase
{
    PackageIdSet known_targets;
    TargetSettings current_settings;
    BuildSettings bs;
    std::vector<ITargetPtr> added_targets;
};

// this driver ep
struct NativeTargetEntryPoint : TargetEntryPoint
{
    ModuleSwappableDataBase module_data;
    path source_dir;

    [[nodiscard]]
    std::vector<ITargetPtr> loadPackages(SwBuild &, const TargetSettings &, const PackageIdSet &pkgs) const override;

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

private:
    const std::unordered_set<LocalPackage> pkgs_;
    mutable Files files_;

    void loadPackages1(Build &) const override;

    SharedLibraryTarget &createTarget(Build &, const Files &files) const;
    decltype(auto) commonActions(Build &, const Files &files) const;
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
    using BuildFunction = void(*)(Build &);
    using CheckFunction = void(*)(Checker &);

    BuildFunction bf = nullptr;
    CheckFunction cf = nullptr;

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
