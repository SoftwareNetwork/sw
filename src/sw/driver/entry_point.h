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

private:
    virtual void loadPackages1(Build &) const = 0;
};

struct PrepareConfigEntryPoint : NativeTargetEntryPoint
{
    mutable path out;
    mutable FilesMap r;
    mutable std::optional<PackageId> tgt;

    // output var
    mutable UnresolvedPackages udeps;

    PrepareConfigEntryPoint(const std::set<Input *> &inputs);

    bool isOutdated() const;

private:
    mutable Files files_;
    const std::set<Input *> &inputs;

    mutable FilesSorted pkg_files_;
    mutable path driver_idir;
    mutable std::set<SharedLibraryTarget *> targets;

    void loadPackages1(Build &) const override;

    SharedLibraryTarget &createTarget(Build &, const Input &) const;
    decltype(auto) commonActions(Build &, const Input &, const UnresolvedPackages &deps) const;
    void commonActions2(Build &, SharedLibraryTarget &lib) const;

    // many input files to many dlls
    void many2many(Build &, const std::set<Input *> &inputs) const;

    // one input file to one dll
    void one2one(Build &, const Input &) const;
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
