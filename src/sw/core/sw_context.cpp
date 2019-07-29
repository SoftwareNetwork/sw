// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "sw_context.h"

#include "build.h"
#include "input.h"
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
    else if (getHostOs().is(OSType::Linux))
    {
        /*ts["native"]["stdlib"]["c"] = "com.Microsoft.Windows.SDK.ucrt";
        ts["native"]["stdlib"]["cpp"] = "com.Microsoft.VisualStudio.VC.libcpp";
        ts["native"]["stdlib"]["kernel"] = "com.Microsoft.Windows.SDK.um";*/

        auto if_add = [this](auto &s, const auto &name)
        {
            if (getPredefinedTargets()[name].empty())
                return false;
            s = name;
            return true;
        };

        if_add(ts["native"]["program"]["c"], "org.gnu.gcc");
        if_add(ts["native"]["program"]["cpp"], "org.gnu.gpp");
        if_add(ts["native"]["program"]["asm"], "org.gnu.gcc.as");
        if_add(ts["native"]["program"]["lib"], "org.gnu.binutils.ar");
        if_add(ts["native"]["program"]["link"], "org.gnu.gcc.ld");
    }
    // add more defaults
    else
        SW_UNIMPLEMENTED;
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

SwContext::SwContext(const path &local_storage_root_dir)
    : SwCoreContext(local_storage_root_dir)
{
}

SwContext::~SwContext()
{
}

std::unique_ptr<SwBuild> SwContext::createBuild()
{
    auto b = std::make_unique<SwBuild>(*this);
    b->getTargets() = getPredefinedTargets();
    return std::move(b);
}

void SwContext::registerDriver(std::unique_ptr<IDriver> driver)
{
    drivers.insert_or_assign(driver->getPackageId(), std::move(driver));
}

}
