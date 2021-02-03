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
struct Resolver;

using ITargetPtr = std::unique_ptr<ITarget>;

struct my_package_transform : package_transform
{
    std::shared_ptr<SwBuild> b;
    ITargetPtr t;

    my_package_transform(const std::shared_ptr<SwBuild> &, const PackageSettings &, const Package &, Input &, Resolver &);
    Commands get_commands() const override;
    const PackageSettings &get_properties() const override;
};

struct my_package_loader : package_loader
{
    PackagePtr p;
    std::shared_ptr<SwBuild> b;
    std::shared_ptr<Input> i;
    std::shared_ptr<Resolver> r;
    mutable std::shared_ptr<package_transform> t;

    my_package_loader(const Package &in) : p(in.clone()) {}
    const PackageName &get_package_name() const override { return p->getId().getName(); }
    std::shared_ptr<package_transform> load(const PackageSettings &) const override;
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
