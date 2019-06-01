// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "sw_context.h"

#include <regex>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "context");

namespace sw
{

IDriver::~IDriver() = default;

SwContext::SwContext(const path &local_storage_root_dir)
    : SwBuilderContext(local_storage_root_dir)
{
    source_dir = fs::canonical(fs::current_path());
}

SwContext::~SwContext()
{
}

void SwContext::build(const path &p)
{
    load(p);
    SW_UNIMPLEMENTED;
    //s->execute();
}

void SwContext::build(const Files &files_or_dirs)
{
    if (files_or_dirs.size() == 1)
        return build(*files_or_dirs.begin());

    // proper multibuilds must get commands and create a single execution plan
    throw SW_RUNTIME_ERROR("not implemented");
}

void SwContext::build(const Strings &strings)
{
    auto inputs = makeInputs(strings);

    /*if (std::all_of(packages.begin(), packages.end(), [](const auto &p)
        {
            return path(p).is_absolute() || fs::exists(p);
        }))
    {
        Files files;
        for (auto &p : packages)
            files.insert(p);
        return build(files);
    }

    StringSet p2;
    for (auto &p : packages)
        p2.insert(p);*/

    SW_UNIMPLEMENTED;
    //auto b = std::make_unique<Build>(swctx);
    //b->build_packages(p2);
}

std::vector<Input> SwContext::makeInputs(const Strings &strings)
{
    std::vector<Input> inputs;
    for (auto &s : strings)
        inputs.emplace_back(s, *this);
    return inputs;
}

void SwContext::build(const String &s)
{
    // local file or dir is preferable rather than some remote pkg
    if (fs::exists(s))
        return build(path(s));
    return build(Strings{ s });
}

std::optional<path> SwContext::findConfig(const path &dir, const FilesOrdered &fe_s)
{
    for (auto &fn : fe_s)
    {
        if (fs::exists(dir / fn))
            return dir / fn;
    }
    return {};
}

std::optional<path> SwContext::resolveConfig(const path &file_or_dir, const FilesOrdered &fe_s)
{
    auto f = file_or_dir;
    if (f.empty())
        f = fs::current_path();
    if (!f.is_absolute())
        f = fs::absolute(f);
    if (fs::is_directory(f))
        return findConfig(f, fe_s);
    return f;
}

void SwContext::load(const path &file_or_dir)
{
    auto fes = getAvailableFrontendConfigFilenames();
    auto f = resolveConfig(file_or_dir, fes);
    if (!f || std::find(fes.begin(), fes.end(), f->filename().u8string()) == fes.end())
    {
        if (f)
        {
            LOG_INFO(logger, "Unknown config, trying in configless mode. Default mode is native (ASM/C/C++)");

            // find in file first: 'sw driver package-id', call that driver on whole file
        }

        path p = file_or_dir;
        p = fs::absolute(p);

        SW_UNIMPLEMENTED;

        /*auto b = std::make_unique<Build>(swctx);
        b->Local = true;
        b->setSourceDirectory(fs::is_directory(p) ? p : p.parent_path());
        b->load(p, true);
        return b;*/
    }

    SW_UNIMPLEMENTED;

    /*auto b = std::make_unique<Build>(swctx);
    b->Local = true;
    b->setSourceDirectory(f.value().parent_path());
    b->load(f.value());
    return b;*/
}

void SwContext::registerDriver(std::unique_ptr<IDriver> driver)
{
    drivers.insert_or_assign(driver->getPackageId(), std::move(driver));
}

FilesOrdered SwContext::getAvailableFrontendConfigFilenames() const
{
    SW_UNIMPLEMENTED;

    /*FilesOrdered s;
    for (auto &[pkg, drv] : drivers)
    {
        auto s2 = drv->getAvailableFrontendConfigFilenames();
        s.insert(s.end(), s2.begin(), s2.end());
    }
    return s;*/
}

Input::Input(const String &s, const SwContext &swctx)
{
    path p = s;
    auto status = fs::status(p);
    if (status.type() == fs::file_type::not_found)
    {
        type = InputType::PackageId;
        subject = PackageId(s).toString(); // force package id check
        return;
    }

    if (p.empty())
        p = fs::current_path();
    if (!p.is_absolute())
        p = fs::absolute(p);

    subject = normalize_path(p);

    auto find_driver = [this, &swctx](auto t)
    {
        type = t;
        for (auto &[_, d] : swctx.getDrivers())
        {
            if (d->canLoad(*this))
            {
                driver = d.get();
                return true;
            }
        }
        return false;
    };

    // spec or regular file
    if (status.type() == fs::file_type::regular)
    {
        if (find_driver(InputType::SpecificationFile) ||
            find_driver(InputType::InlineSpecification))
            return;

        // find in file first: 'sw driver package-id', call that driver on whole file
        auto f = read_file(p);

        static const std::regex r("sw\\s+driver\\s+(\\S+)");
        std::smatch m;
        if (std::regex_search(f, m, r))
        {
            SW_UNIMPLEMENTED;

            /*
                - install driver
                - load & register it
                - re-run this ctor
            */

            auto driver_pkg = swctx.install({ m[1].str() })[m[1].str()];
            return;
        }
    }
    else if (status.type() == fs::file_type::directory)
    {
        if (find_driver(InputType::DirectorySpecificationFile) ||
            find_driver(InputType::Directory))
            return;
    }
    else
        throw SW_RUNTIME_ERROR("Bad file type: " + s);

    throw SW_RUNTIME_ERROR("Cannot select driver for " + subject);
}

}

