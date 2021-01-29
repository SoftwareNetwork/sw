// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2021 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <sw/core/package.h>

#include <memory>

namespace sw
{

struct SwBuild;
struct Input;
struct ITarget;

using ITargetPtr = std::unique_ptr<ITarget>;

struct my_package_transform : package_transform
{
    ITargetPtr t;

    Commands get_commands() const override;
    const PackageSettings &get_properties() const override;
};

struct my_package_loader : package_loader
{
    PackageName p;
    std::unique_ptr<SwBuild> b;
    std::shared_ptr<Input> i;

    my_package_loader(const PackageName &in) : p(in) {}
    const PackageName &get_package_name() const override { return p; }
    std::unique_ptr<package_transform> load(const PackageSettings &s) const override;
};

struct my_physical_package : physical_package
{
    ITargetPtr t;
    PackageId p;

    my_physical_package(ITargetPtr in);

    const PackageId &get_package() const override { return p; }
    const PackageSettings &get_properties() const override;
};

} // namespace sw
