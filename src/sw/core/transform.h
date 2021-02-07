// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2021 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <sw/manager/package.h>

namespace sw
{

struct IDriver;
struct package_loader;
struct package_transform;

struct SW_CORE_API transform
{
    using package_loader_ptr = std::unique_ptr<package_loader>;
    using package_transform_ptr = std::unique_ptr<package_transform>;
    using drivers = std::map<PackageName, std::unique_ptr<IDriver>>;

    transform() = default;
    transform(const transform &) = delete;
    transform &operator=(const transform &) = delete;

    void add_driver(const PackageName &pkg, std::unique_ptr<IDriver> driver);

    package_loader *load_package(const Package &); // load installed package
    std::vector<package_loader *> load_packages(const path &); // load from path

    //package_transform *add_transform(package_loader &, const PackageSettings &);

private:
    drivers drivers_;
    std::vector<package_loader_ptr> package_loaders;
    //std::vector<package_transform_ptr> package_transforms;
};

} // namespace sw
