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

//SW_REGISTER_PACKAGE_DRIVER(Driver);

path Driver::getConfigFilename() const
{
    return Build::getConfigFilename();
}

PackageScriptPtr Driver::build(const path &file_or_dir) const
{
    auto f = file_or_dir;
    if (fs::is_directory(f))
    {
        if (!hasConfig(f))
            return {};
        f /= getConfigFilename();
    }

    current_thread_path(f.parent_path());

    auto b = std::make_unique<Build>();
    b->Local = true;
    b->configure = true;
    b->build(f);
    return b;
}

PackageScriptPtr Driver::load(const path &file_or_dir) const
{
    auto f = file_or_dir;
    if (fs::is_directory(f))
    {
        if (!hasConfig(f))
            return {};
        f /= getConfigFilename();
    }

    current_thread_path(f.parent_path());

    auto b = std::make_unique<Build>();
    b->Local = true;
    b->configure = true;
    b->build_and_load(f);

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

bool Driver::execute(const path &file_or_dir) const
{
    if (auto b = load(file_or_dir); b)
        return b->execute();
    return false;
}

static auto fetch1(const Driver *driver, const path &file_or_dir)
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

    Solution::SourceDirMapBySource srcs_old;
    while (1)
    {
        auto b = std::make_unique<Build>();
        //b->Local = true;
        b->perform_checks = false;
        b->PostponeFileResolving = true;
        b->source_dirs_by_source = srcs_old;
        b->build_and_load(f);

        Solution::SourceDirMapBySource srcs;
        for (const auto &[pkg, t] : b->solutions.begin()->getChildren())
        {
            auto s = t->source; // make a copy!
            checkSourceAndVersion(s, pkg.getVersion());
            srcs[s] = d / get_source_hash(s);
        }

        // src_old has correct root dirs
        if (srcs.size() == srcs_old.size())
            return std::tuple{ std::move(b), srcs_old };

        auto &e = getExecutor();
        Futures<void> fs;
        for (auto &src : srcs)
        {
            fs.push_back(e.push([src = src.first, &d = src.second]
            {
                if (!fs::exists(d))
                {
                    LOG_INFO(logger, "Downloading source:\n" << print_source(src));
                    fs::create_directories(d);
                    ScopedCurrentPath scp(d, CurrentPathScope::Thread);
                    download(src);
                }
                d = d / findRootDirectory(d); // pass found regex or files for better root dir lookup
            }));
        }
        waitAndGet(fs);

        srcs_old = srcs;
    }
}

void Driver::fetch(const path &file_or_dir) const
{
    fetch1(this, file_or_dir);
}

PackageScriptPtr Driver::fetch_and_load(const path &file_or_dir) const
{
    auto [b, srcs] = fetch1(this, file_or_dir);

    // sources are fetched, root dirs are known
    // now we reload with passing that info to driver

    auto &e = getExecutor();
    Futures<void> fs;
    for (const auto &[pkg, t] : b->solutions.begin()->getChildren())
    {
        fs.push_back(e.push([t, &srcs, &pkg]
        {
            auto s2 = t->source;
            applyVersionToUrl(s2, pkg.version);
            auto i = srcs.find(s2);
            path rd = i->second / t->RootDirectory;
            ScopedCurrentPath scp(rd, CurrentPathScope::Thread);

            t->SourceDir = rd;
            t->prepare();
        }));
    }
    waitAndGet(fs);

    return std::move(b);
}

} // namespace sw::driver
