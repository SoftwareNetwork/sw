// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "driver.h"

#include "build.h"
#include "module.h"

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "driver.cpp");

namespace sw
{

std::optional<path> findConfig(const path &dir, const FilesOrdered &fe_s)
{
    for (auto &fn : fe_s)
    {
        if (fs::exists(dir / fn))
            return dir / fn;
    }
    return {};
}

namespace driver::cpp
{

Driver::Driver(SwContext &swctx)
    : swctx(swctx)
{
    build = std::make_unique<Build>(swctx, *this);

    //source_dir = fs::canonical(fs::current_path());
    module_storage = std::make_unique<ModuleStorage>();
}

Driver::~Driver()
{
    // do not clear modules on exception, because it may come from there
    // TODO: cleanup modules data first
    if (std::uncaught_exceptions())
        module_storage.release();
}

PackageId Driver::getPackageId() const
{
    return "org.sw.sw.driver.cpp-0.3.0";
}

bool Driver::canLoad(const Input &i) const
{
    switch (i.getType())
    {
    case InputType::SpecificationFile:
    {
        auto &fes = Build::getAvailableFrontendConfigFilenames();
        return std::find(fes.begin(), fes.end(), i.getPath().filename().u8string()) != fes.end();
    }
    case InputType::InlineSpecification:
        SW_UNIMPLEMENTED;
        break;
    case InputType::DirectorySpecificationFile:
        return !!findConfig(i.getPath(), Build::getAvailableFrontendConfigFilenames());
    case InputType::Directory:
        SW_UNIMPLEMENTED;
        break;
    default:
        SW_UNREACHABLE;
    }
    return false;
}

void Driver::load(const std::set<Input> &inputs)
{
    std::unordered_set<LocalPackage> pkgs;
    for (auto &i : inputs)
    {
        switch (i.getType())
        {
        case InputType::InstalledPackage:
        {
            pkgs.insert(swctx.resolve(i.getPackageId()));
            break;
        }
        case InputType::DirectorySpecificationFile:
        {
            auto p = *findConfig(i.getPath(), Build::getAvailableFrontendConfigFilenames());
            build->load_spec_file(p);
            break;
        }
        default:
            SW_UNREACHABLE;
        }
    }

    if (!pkgs.empty())
    {
        Build b(swctx, *this); // cache?
        //b.execute_jobs = config_jobs;
        b.file_storage_local = false;
        b.is_config_build = true;
        b.use_separate_target_map = true;

        b.build_configs(pkgs);

        /*auto get_package_config = [](const auto &pkg) -> path
        {
            if (pkg.getData().group_number)
            {
                auto d = findConfig(pkg.getDirSrc2(), Build::getAvailableFrontendConfigFilenames());
                if (!d)
                    throw SW_RUNTIME_ERROR("cannot find config");
                return *d;
            }
            auto p = pkg.getGroupLeader();
            if (auto d = findConfig(p.getDirSrc2(), Build::getAvailableFrontendConfigFilenames()))
                return *d;
            auto d = findConfig(pkg.getDirSrc2(), Build::getAvailableFrontendConfigFilenames());
            if (!d)
                throw SW_RUNTIME_ERROR("cannot find config");
            fs::create_directories(p.getDirSrc2());
            fs::copy_file(*d, p.getDirSrc2() / d->filename());
            return p.getDirSrc2() / d->filename();
        };

        Files files;
        std::unordered_map<path, LocalPackage> output_names;
        for (auto &pkg : pkgs)
        {
            auto p = get_package_config(pkg);
            files.insert(p);
            output_names.emplace(p, pkg);
        }
        b.build_configs_separate(files);*/
    }
}

void Driver::execute()
{
    build->execute();
}

bool Driver::prepareStep()
{
    return build->prepareStep();
}

ChecksStorage &Driver::getChecksStorage(const String &config) const
{
    auto i = checksStorages.find(config);
    if (i == checksStorages.end())
    {
        auto [i, _] = checksStorages.emplace(config, std::make_unique<ChecksStorage>());
        return *i->second;
    }
    return *i->second;
}

ChecksStorage &Driver::getChecksStorage(const String &config, const path &fn) const
{
    auto i = checksStorages.find(config);
    if (i == checksStorages.end())
    {
        auto [i, _] = checksStorages.emplace(config, std::make_unique<ChecksStorage>());
        i->second->load(fn);
        return *i->second;
    }
    return *i->second;
}

ModuleStorage &Driver::getModuleStorage() const
{
    return *module_storage;
}

}

}
