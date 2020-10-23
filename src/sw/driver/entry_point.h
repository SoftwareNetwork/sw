// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

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
struct DriverData;
struct Input;

// this driver ep
struct NativeTargetEntryPoint : TargetEntryPoint
{
    path source_dir;
    mutable std::unique_ptr<DriverData> dd;

    [[nodiscard]]
    std::vector<ITargetPtr> loadPackages(SwBuild &, const TargetSettings &, const AllowedPackages &pkgs, const PackagePath &prefix) const override;

    Build createBuild(SwBuild &, const TargetSettings &, const AllowedPackages &pkgs, const PackagePath &prefix) const;

private:
    virtual void loadPackages1(Build &) const {}
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
    const Module &m;

    void loadPackages1(Build &) const override;
};

struct PrepareConfigOutputData
{
    path dll;
    FilesOrdered PATH;

    template <class Ar>
    void serialize(Ar & ar, unsigned)
    {
        ar & dll;
        ar & PATH;
    }
};

struct PrepareConfig
{
    struct InputData
    {
        path fn;
        path cfn;
        String cl_name;
        String link_name;
    };
    using FilesMap = std::unordered_map<path, PrepareConfigOutputData>;

    FilesMap r;
    std::optional<PackageId> tgt;
    enum
    {
        LANG_CPP,
        LANG_C,
        LANG_VALA
    } lang;
    std::set<SharedLibraryTarget *> targets;

    // output var
    //mutable UnresolvedPackages udeps;

    void addInput(Build &, const Input &);
    bool isOutdated() const;

private:
    bool inputs_outdated = false;
    path driver_idir;

    SharedLibraryTarget &createTarget(Build &, const InputData &);
    decltype(auto) commonActions(Build &, const InputData &, const UnresolvedPackages &deps);

    // one input file to one dll
    path one2one(Build &, const InputData &);
};

}
