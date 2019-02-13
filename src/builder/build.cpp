// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <sw/builder/build.h>

#include <sw/builder/driver.h>

#include <directories.h>
#include <resolver.h>

#include <primitives/date_time.h>
#include <primitives/lock.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "build");

//#include <suffix.h>

namespace sw
{

bool build(const path &p)
{
    // find available drivers
    // find software rules
    // select driver
    // run

    auto &drivers = sw::getDrivers();
    for (auto &d : drivers)
    {
        if (d->execute(p))
        {
            return true;
        }
    }

    // no appropriate driver found
    return false;
}

bool build(const Files &files_or_dirs)
{
    if (files_or_dirs.size() == 1)
        return build(*files_or_dirs.begin());

    // proper multibuilds must get commands and create a single execution plan
    throw SW_RUNTIME_ERROR("not implemented");
    return true;
}

bool build(const Strings &packages)
{
    if (std::all_of(packages.begin(), packages.end(), [](const auto &p)
    {
        return fs::exists(p);
    }))
    {
        Files files;
        for (auto &p : packages)
            files.insert(p);
        return build(files);
    }

    StringSet p2;
    for (auto &p : packages)
        p2.insert(p);

    auto &drivers = sw::getDrivers();
    for (auto &d : drivers)
    {
        if (d->buildPackages(p2))
        {
            return true;
        }
    }
    return true;
}

bool build(const String &s)
{
    // local file or dir is preferable rather than some remote pkg
    if (fs::exists(s))
        return build(path(s));
    return build({ s });
}

PackageScriptPtr build_only(const path &file_or_dir)
{
    auto &drivers = getDrivers();
    for (auto &d : drivers)
    {
        if (auto s = d->build(file_or_dir); s)
            return s;
    }
    throw SW_RUNTIME_ERROR("Unknown package driver");
}

PackageScriptPtr load(const path &file_or_dir)
{
    auto &drivers = getDrivers();
    for (auto &d : drivers)
    {
        if (auto s = d->load(file_or_dir); s)
            return s;
    }
    throw SW_RUNTIME_ERROR("Unknown package driver");
}

PackageScriptPtr fetch_and_load(const path &file_or_dir, const FetchOptions &opts)
{
    auto &drivers = getDrivers();
    for (auto &d : drivers)
    {
        if (auto s = d->fetch_and_load(file_or_dir, opts); s)
            return s;
    }
    throw SW_RUNTIME_ERROR("Unknown package driver");
}

DriverPtr loadDriver(const path &file_or_dir)
{
    auto &drivers = getDrivers();
    for (auto &d : drivers)
    {
        //if (auto s = d->load(file_or_dir); s)
            //return d;
    }
    return nullptr;
}

bool run(const PackageId &package)
{
    auto &drivers = getDrivers();
    for (auto &d : drivers)
    {
        if (auto s = d->run(package); s)
            return s;
    }
    throw SW_RUNTIME_ERROR("Unknown package driver");
}

std::optional<String> read_config(const path &file_or_dir)
{
    auto &drivers = getDrivers();
    for (auto &d : drivers)
    {
        if (auto s = d->readConfig(file_or_dir); s)
            return s;
    }
    return {};
}

}
