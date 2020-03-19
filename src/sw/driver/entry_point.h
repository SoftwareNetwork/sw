/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2020 Egor Pugin
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
    std::vector<ITargetPtr> loadPackages(SwBuild &, const TargetSettings &, const PackageIdSet &pkgs, const PackagePath &prefix) const override;

    Build createBuild(SwBuild &, const TargetSettings &, const PackageIdSet &pkgs, const PackagePath &prefix) const;

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
    Module m;

    void loadPackages1(Build &) const override;
};

struct PrepareConfigOutputData
{
    path dll;
    FilesOrdered PATH;
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
    bool vala = false;
    std::set<SharedLibraryTarget *> targets; // internal

    // output var
    mutable UnresolvedPackages udeps;

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
