// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2021 Egor Pugin <egor.pugin@gmail.com>

#include "package.h"

#include "input.h"

#include <sw/core/build.h>
#include <sw/core/target.h>

namespace sw
{

Commands my_package_transform::get_commands() const { return t->getCommands(); }

const PackageSettings &my_package_transform::get_properties() const { return t->getInterfaceSettings(); }

std::unique_ptr<package_transform> my_package_loader::load(const PackageSettings &s) const
{
    auto t = i->loadPackage(*b, s, *p);
    auto pt = std::make_unique<my_package_transform>();
    pt->t = std::move(t);
    pt->b = b;
    return pt;
}

my_physical_package::my_physical_package(ITargetPtr in) : t(std::move(in)), p{ t->getPackage(), t->getSettings() } {}

const PackageSettings &my_physical_package::get_properties() const { return t->getInterfaceSettings(); }

}
