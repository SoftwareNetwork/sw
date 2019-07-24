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

struct ModuleSwappableData
{
    NativeTargetEntryPoint *ntep = nullptr;
    PackagePath NamePrefix;
    PackageVersionGroupNumber current_gn = 0;
    TargetSettings current_settings;
    BuildSettings bs;
    PackageIdSet known_targets;
};

// this driver ep
struct NativeTargetEntryPoint : TargetEntryPoint,
    std::enable_shared_from_this<NativeTargetEntryPoint>
{
    mutable ModuleSwappableData module_data;

    void loadPackages(SwBuild &, const TargetSettings &, const PackageIdSet &pkgs) const override;

private:
    virtual void loadPackages1(Build &) const = 0;
};

struct PrepareConfigEntryPoint : NativeTargetEntryPoint
{
    using NativeTargetEntryPoint::NativeTargetEntryPoint;

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
