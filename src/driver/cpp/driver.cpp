// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <sw/driver/cpp/driver.h>

#include <filesystem.h>
#include <package_data.h>
#include <solution.h>

#include <primitives/lock.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "driver.cpp");

namespace sw::driver::cpp
{

//SW_REGISTER_PACKAGE_DRIVER(CppDriver);

path CppDriver::getConfigFilename() const
{
    return Build::getConfigFilename();
}

optional<path> CppDriver::resolveConfig(const path &file_or_dir) const
{
    auto f = file_or_dir;
    if (fs::is_directory(f))
    {
        if (!hasConfig(f))
            return {};
        f /= getConfigFilename();
    }
    return f;
}

PackageScriptPtr CppDriver::build(const path &file_or_dir) const
{
    auto f = resolveConfig(file_or_dir);
    if (!f)
        return {};
    current_thread_path(f.value().parent_path());

    auto b = std::make_unique<Build>();
    b->Local = true;
    b->configure = true;
    b->build(f.value());
    return b;
}

PackageScriptPtr CppDriver::load(const path &file_or_dir) const
{
    auto f = resolveConfig(file_or_dir);
    if (!f)
        return {};
    current_thread_path(f.value().parent_path());

    auto b = std::make_unique<Build>();
    b->Local = true;
    b->configure = true;
    b->build_and_load(f.value());

    // in order to discover files, deps?
    //b->prepare();

    /*single_process_job(".sw/build", [&f]()
    {
        Build b;
        b.Local = true;
        b.configure = true;
        b.build_and_run(f);

        //LOG_INFO(logger, "Total time: " << t.getTimeFloat());
    });*/

    return b;
}

bool CppDriver::execute(const path &file_or_dir) const
{
    auto f = resolveConfig(file_or_dir);
    if (!f)
        return {};
    current_thread_path(f.value().parent_path());

    if (auto s = load(f.value()); s)
        return s->execute();
    return false;
}

static auto fetch1(const CppDriver *driver, const path &file_or_dir)
{
    auto f = file_or_dir;
    if (fs::is_directory(f))
    {
        if (!driver->hasConfig(f))
            throw std::runtime_error("no config found");
        f /= driver->getConfigFilename();
    }

    auto d = file_or_dir;
    if (!fs::is_directory(d))
        d = d.parent_path();
    d = d / ".sw" / "src";

    auto b = std::make_unique<Build>();
    //b->Local = false;
    b->perform_checks = false;
    //b->PostponeFileResolving = true;
    b->fetch_dir = d;
    b->build_and_load(f);

    // reset
    b->fetch_dir.clear();
    for (auto &s : b->solutions)
        s.fetch_dir.clear();

    return std::move(b);
}

void CppDriver::fetch(const path &file_or_dir) const
{
    fetch1(this, file_or_dir);
}

PackageScriptPtr CppDriver::fetch_and_load(const path &file_or_dir) const
{
    auto b = fetch1(this, file_or_dir);

    // do not use b->prepare(); !
    // prepare only packages in solution
    auto &e = getExecutor();
    Futures<void> fs;
    for (const auto &[pkg, t] : b->solutions.begin()->getChildren())
    {
        fs.push_back(e.push([t] {
            t->prepare();
        }));
    }
    waitAndGet(fs);

    return std::move(b);
}

bool CppDriver::buildPackage(const PackageId &pkg) const
{
    auto b = std::make_unique<Build>();
    b->build_package(pkg.toString());
    return true;
}

bool CppDriver::run(const PackageId &pkg) const
{
    auto b = std::make_unique<Build>();
    b->run_package(pkg.toString());
    return true;
}

} // namespace sw::driver
