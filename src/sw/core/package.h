// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2021 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <sw/builder/command.h>
#include <sw/manager/package.h>

namespace sw
{

struct package_transform;

struct SW_CORE_API package_loader
{
    virtual ~package_loader() = 0;

    virtual const PackageName &get_package_name() const = 0;
    virtual const package_transform &load(const PackageSettings &) = 0;
    //virtual std::vector<const package_transform *> get_package_transforms() const = 0;
};

// local pkg?
struct SW_CORE_API physical_package
{
    //physical_package(const PackageId &p) : p(p) {}
    virtual ~physical_package() = 0;

    virtual const PackageId &get_package() const = 0;// { return p; }

    // get_files
    // get_source
    // get_properties()
    // pass files and source via properties?

    // public properties
    virtual const PackageSettings &get_properties() const = 0;

    // api
    //virtual std::unique_ptr<package_transform> transform(const PackageSettings &) const = 0;

private:
    // pkg instead of id?
    //PackagePtr p;
    //PackageId p;
};

// promise?
struct SW_CORE_API package_transform
{
    virtual ~package_transform() = 0;
    //
    virtual Commands get_commands() const = 0;

    // perform ourselves
    // reconsider? remove? add transform_helper or transform_executor that will execute transform plan (commands)
    //virtual physical_package perform(/*builder context or executor*/) = 0;

    // we also need properties here to be able to use them from promise (as dependency)
    virtual const PackageSettings &get_properties() const = 0;
};

struct SW_CORE_API transform_executor
{
    // nthreads
    // ...

    void execute(const std::vector<const package_transform *> &);
};

} // namespace sw
