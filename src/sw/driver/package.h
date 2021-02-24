// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2021 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <sw/core/package.h>

#include <memory>

namespace sw
{

struct ExtendedBuild;
struct SwBuild;
struct Input;
struct ITarget;
struct Resolver;
struct Build;

using ITargetPtr = std::unique_ptr<ITarget>;

struct SW_DRIVER_CPP_API my_package_transform : package_transform
{
    ITargetPtr t;

    my_package_transform(Build &, const Package &, Input &);
    Commands get_commands() const override;
    const PackageSettings &get_properties() const override;
};

struct SW_DRIVER_CPP_API my_package_loader : package_loader
{
    PackagePtr p;
    std::shared_ptr<SwBuild> build;
    std::shared_ptr<ExtendedBuild> build2;
    std::shared_ptr<Input> input;
    std::shared_ptr<Resolver> resolver;
    std::unordered_map<size_t, std::unique_ptr<package_transform>> transforms;

    my_package_loader(const Package &in) : p(in.clone()) {}
    const PackageName &get_package_name() const override { return p->getId().getName(); }
    const package_transform &load(const PackageSettings &) override;
};

struct SW_DRIVER_CPP_API my_physical_package : physical_package
{
    ITargetPtr t;
    PackageId p;

    my_physical_package(ITargetPtr in);

    const PackageId &get_package() const override { return p; }
    const PackageSettings &get_properties() const override;
};

} // namespace sw
