// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2021 Egor Pugin <egor.pugin@gmail.com>

#include "package.h"

#include "input.h"

#include <sw/core/build.h>
#include <sw/core/target.h>

#include <boost/fiber/all.hpp>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "driver.package");

namespace sw
{

my_package_transform::my_package_transform(const std::shared_ptr<SwBuild> &inb, const PackageSettings &s, const Package &p, Input &i, Resolver &r)
    : b(inb)
{
    std::exception_ptr eptr;
    auto x = [this, &eptr, &s, &p, &i, &r]()
    {
        LOG_TRACE(logger, "Entering the new fiber to load: " + p.getId().toString());
        try
        {
            t = i.loadPackage(*b, r, s, p);
        }
        catch (...)
        {
            eptr = std::current_exception();
        }
        LOG_TRACE(logger, "Leaving fiber to load: " + p.getId().toString());
    };
    // boost::fibers::launch::dispatch,
    // std::allocator_arg_t
    // add stack 2 MB
    boost::fibers::fiber f(x);
    f.join();
    if (eptr)
        std::rethrow_exception(eptr);
}

Commands my_package_transform::get_commands() const { return t->getCommands(); }

const PackageSettings &my_package_transform::get_properties() const { return t->getInterfaceSettings(); }

const package_transform &my_package_loader::load(const PackageSettings &s)
{
    auto h = s.getHash();
    auto i = transforms.find(h);
    if (i != transforms.end())
        return *i->second;
    auto [j,_] = transforms.emplace(h, std::make_unique<my_package_transform>(build, s, *p, *input, *resolver));
    return *j->second;
}

my_physical_package::my_physical_package(ITargetPtr in) : t(std::move(in)), p{ t->getPackage(), t->getSettings() } {}

const PackageSettings &my_physical_package::get_properties() const { return t->getInterfaceSettings(); }

}
