// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2021 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <sw/manager/package.h>

#include <memory>

namespace sw
{

struct package_loader;
struct package_transform;
struct IDriver;

struct SW_CORE_API transform
{
    using package_loader_ptr = std::unique_ptr<package_loader>;
    //using package_transform_ptr = std::unique_ptr<package_transform>;
    // driver is a physical_package that is able to load other packages
    //using drivers = std::map<PackageName, IDriver *>;

    // we need to accept basic driver here
    transform();
    transform(const transform &) = delete;
    transform &operator=(const transform &) = delete;
    ~transform();

    void add_driver(IDriver &driver);

    package_loader *load_package(const Package &); // load installed package
    std::vector<package_loader *> load_packages(const path &); // load from path

    //package_transform *add_transform(package_loader &, const PackageSettings &);

private:
    std::map<PackageName, IDriver *> drivers;
    std::map<PackageName, package_loader *> package_loaders;
    //std::vector<package_transform_ptr> package_transforms;
};

struct SW_CORE_API transform_executor
{
    // nthreads
    // ...

    void execute(const std::vector<const package_transform *> &);
};

} // namespace sw
