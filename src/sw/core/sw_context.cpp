// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "sw_context.h"

#include "build.h"
#include "driver.h"

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "context");

namespace sw
{

void detectCompilers(SwCoreContext &s);

IDriver::~IDriver() = default;

SwCoreContext::SwCoreContext(const path &local_storage_root_dir)
    : SwBuilderContext(local_storage_root_dir)
{
    source_dir = primitives::filesystem::canonical(fs::current_path());

    // before detection
    createHostSettings();
    detectCompilers(*this);
    // after detection
    setHostPrograms();
}

SwCoreContext::~SwCoreContext()
{
}

void SwCoreContext::createHostSettings()
{
    host_settings = toTargetSettings(getHostOs());

    auto &ts = host_settings;
    ts["native"]["configuration"] = "release";
    ts["native"]["library"] = "shared";
}

void SwCoreContext::setHostPrograms()
{
    host_settings = toTargetSettings(getHostOs());

    auto &ts = host_settings;
    ts["native"]["configuration"] = "release";
    ts["native"]["library"] = "shared";

    if (getHostOs().is(OSType::Windows))
    {
        ts["native"]["stdlib"]["c"] = "com.Microsoft.Windows.SDK.ucrt";
        ts["native"]["stdlib"]["cpp"] = "com.Microsoft.VisualStudio.VC.libcpp";
        ts["native"]["stdlib"]["kernel"] = "com.Microsoft.Windows.SDK.um";

        if (!getPredefinedTargets()["com.Microsoft.VisualStudio.VC.cl"].empty())
        {
            ts["native"]["program"]["c"] = "com.Microsoft.VisualStudio.VC.cl";
            ts["native"]["program"]["cpp"] = "com.Microsoft.VisualStudio.VC.cl";
            ts["native"]["program"]["asm"] = "com.Microsoft.VisualStudio.VC.ml";
            ts["native"]["program"]["lib"] = "com.Microsoft.VisualStudio.VC.lib";
            ts["native"]["program"]["link"] = "com.Microsoft.VisualStudio.VC.link";
        }
        // add more defaults
        else
            SW_UNIMPLEMENTED;
    }
    // add more defaults
    else
        SW_UNIMPLEMENTED;
}

const TargetSettings &SwCoreContext::getHostSettings() const
{
    return host_settings;
}

TargetData &SwCoreContext::getTargetData(const PackageId &pkg)
{
    return target_data[pkg];
}

const TargetData &SwCoreContext::getTargetData(const PackageId &pkg) const
{
    auto i = target_data.find(pkg);
    if (i == target_data.end())
        throw SW_RUNTIME_ERROR("No target data for package: " + pkg.toString());
    return i->second;
}

void SwCoreContext::loadPackages(TargetMap &targets) const
{
    loadPackages(targets, getPredefinedTargets());
}

void SwCoreContext::loadPackages(TargetMap &targets, const TargetMap &predefined) const
{
    // first, we create all package ids with EPs in targets
    for (auto &[p, _] : target_data)
        targets[p];

    // load
    while (1)
    {
        std::map<TargetSettings, std::pair<PackageId, TargetContainer *>> load;
        auto &chld = targets; // take a ref, because it won't be changed in this loop
        for (const auto &[pkg, tgts] : chld)
        {
            for (const auto &tgt : tgts)
            {
                auto deps = tgt->getDependencies();
                for (auto &d : deps)
                {
                    if (d->isResolved())
                        continue;

                    auto i = chld.find(d->getUnresolvedPackage());
                    if (i == chld.end())
                        throw SW_RUNTIME_ERROR("No target loaded: " + d->getUnresolvedPackage().toString());

                    auto k = i->second.find(d->getSettings());
                    if (k != i->second.end())
                    {
                        d->setTarget(**k);
                        continue;
                    }

                    if (predefined.find(d->getUnresolvedPackage().ppath) != predefined.end(d->getUnresolvedPackage().ppath))
                    {
                        throw SW_LOGIC_ERROR(tgt->getPackage().toString() + ": predefined target is not resolved: " + d->getUnresolvedPackage().toString());
                    }

                    load.insert({ d->getSettings(), { i->first, &i->second } });
                }
            }
        }
        if (load.empty())
            break;
        for (auto &[s, d] : load)
        {
            // empty settings mean we want dependency only to be present
            if (s.empty())
                continue;

            getTargetData(d.first).loadPackages(targets, s, {}/* { d.first }*/ );
            auto k = d.second->find(s);
            if (k == d.second->end())
            {
                //throw SW_RUNTIME_ERROR("cannot load package with current settings:\n" + s.toString());
                throw SW_RUNTIME_ERROR("cannot load package " + d.first.toString() + " with current settings\n" + s.toString());
            }
        }
    }
}

SwContext::SwContext(const path &local_storage_root_dir)
    : SwCoreContext(local_storage_root_dir)
{
}

SwContext::~SwContext()
{
}

SwBuild SwContext::createBuild()
{
    SwBuild b(*this);
    b.getTargets() = getPredefinedTargets();
    return b;
}

void SwContext::registerDriver(std::unique_ptr<IDriver> driver)
{
    drivers.insert_or_assign(driver->getPackageId(), std::move(driver));
}

}

