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

FilesOrdered CppDriver::getAvailableFrontends() const
{
    return Build::getAvailableFrontendConfigFilenames();
}

std::optional<path> CppDriver::resolveConfig(const path &file_or_dir) const
{
    auto f = file_or_dir;
    if (f.empty())
        f = fs::current_path();
    if (!f.is_absolute())
        f = fs::absolute(f);
    if (fs::is_directory(f))
        return findConfig(f);
    return f;
}

// build means build config!!!
// remove, it means nothing for user!
PackageScriptPtr CppDriver::build(const path &file_or_dir) const
{
    auto f = resolveConfig(file_or_dir);
    if (!f || !Build::isFrontendConfigFilename(f.value()))
        return {};

    auto b = std::make_unique<Build>();
    b->Local = true;
    //b->configure = true;
    b->build(f.value());
    return b;
}

PackageScriptPtr CppDriver::load(const path &file_or_dir) const
{
    auto f = resolveConfig(file_or_dir);
    if (!f || !Build::isFrontendConfigFilename(f.value()))
    {
        if (f && !Build::isFrontendConfigFilename(f.value()))
            LOG_INFO(logger, "Unknown config, trying in configless mode. Default mode is native (ASM/C/C++)");

        path p = file_or_dir;
        if (fs::is_directory(p))
            ;
        else
            p = p.parent_path();

        auto b = std::make_unique<Build>();
        b->Local = true;
        b->SourceDir = p;
        b->load(p, true);
        return b;
    }

    auto b = std::make_unique<Build>();
    b->Local = true;
    b->SourceDir = f.value().parent_path();
    b->load(f.value());

    return b;
}

bool CppDriver::execute(const path &file_or_dir) const
{
    if (auto s = load(file_or_dir); s)
        return s->execute();
    return false;
}

static auto fetch1(const CppDriver *driver, const path &fn, const FetchOptions &opts, bool parallel)
{
    auto d = fn.parent_path() / SW_BINARY_DIR / "src";

    SourceDirMap srcs_old;
    if (parallel)
    {
        bool pp = true; // postpone once!
        while (1)
        {
            auto b = std::make_unique<Build>();
            b->NamePrefix = opts.name_prefix;
            b->perform_checks = false;
            b->DryRun = true;
            b->PostponeFileResolving = pp;
            b->source_dirs_by_source = srcs_old;
            b->prefix_source_dir = opts.source_dir;
            if (!pp)
                b->fetch_dir = d;
            b->load(fn);

            SourceDirMap srcs;
            for (const auto &[pkg, t] : b->solutions.begin()->getChildren())
            {
                auto s = t->source; // make a copy!
                checkSourceAndVersion(s, pkg.getVersion());
                if (!isValidSourceUrl(s))
                    throw SW_RUNTIME_ERROR("Invalid source: " + print_source(s));
                srcs[s] = d / get_source_hash(s);
            }

            // src_old has correct root dirs
            if (srcs.size() == srcs_old.size())
            {
                if (srcs.size() == 0)
                    throw SW_RUNTIME_ERROR("no sources found");

                // reset
                b->fetch_dir.clear();
                for (auto &s : b->solutions)
                    s.fetch_dir.clear();

                b->fetch_info.sources = srcs_old;

                return b;
            }

            // with this, we only have two iterations
            // This is a limitation, but on the other hand handling of this
            // become too complex for now.
            // For other cases uses non-parallel mode.
            pp = false;

            download(srcs, opts);
            srcs_old = srcs;
        }
    }
    else
    {
        auto b = std::make_unique<Build>();
        b->NamePrefix = opts.name_prefix;
        b->perform_checks = false;
        b->DryRun = true;
        b->fetch_dir = d;
        b->prefix_source_dir = opts.source_dir;
        b->load(fn);

        // reset
        b->fetch_dir.clear();
        for (auto &s : b->solutions)
            s.fetch_dir.clear();

        b->fetch_info.sources = srcs_old;

        return b;
    }
}

void CppDriver::fetch(const path &file_or_dir, const FetchOptions &opts, bool parallel) const
{
    auto f = resolveConfig(file_or_dir);
    if (!f || !Build::isFrontendConfigFilename(f.value()))
        throw SW_RUNTIME_ERROR("no config found");

    fetch1(this, f.value(), opts, parallel);
}

PackageScriptPtr CppDriver::fetch_and_load(const path &file_or_dir, const FetchOptions &opts, bool parallel) const
{
    auto f = resolveConfig(file_or_dir);
    if (!f || !Build::isFrontendConfigFilename(f.value()))
        throw SW_RUNTIME_ERROR("no config found");

    auto b = fetch1(this, f.value(), opts, parallel);

    if (parallel)
    for (const auto &[pkg, t] : b->solutions.begin()->getChildren())
    {
        auto s = t->source; // make a copy!
        applyVersionToUrl(s, pkg.version);
        if (opts.apply_version_to_source)
            applyVersionToUrl(t->source, pkg.version);
        auto i = b->fetch_info.sources.find(s);
        path rd = i->second/* / t->RootDirectory*/;
        t->SourceDir = rd;
    }

    b->prepareStep();

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
