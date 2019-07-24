// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "target.h"

#include <sw/builder/sw_context.h>

namespace sw
{

struct IDriver;
struct SwBuild;

// core context for drivers
struct SW_CORE_API SwCoreContext : SwBuilderContext
{
    // move to drivers? remove?
    path source_dir;

    SwCoreContext(const path &local_storage_root_dir);
    virtual ~SwCoreContext();

    TargetMap &getPredefinedTargets() { return predefined_targets; }
    const TargetMap &getPredefinedTargets() const { return predefined_targets; }
    const TargetSettings &getHostSettings() const;

    const std::unordered_map<PackageId, TargetData> &getTargetData() const { return target_data; }
    TargetData &getTargetData(const PackageId &);
    const TargetData &getTargetData(const PackageId &) const;

private:
    TargetMap predefined_targets;
    TargetSettings host_settings;
    std::unordered_map<PackageId, TargetData> target_data;
    // also target data: entry point + common data

    void createHostSettings();
    void setHostPrograms();
};

// public context
struct SW_CORE_API SwContext : SwCoreContext
{
    using Drivers = std::map<PackageId, std::unique_ptr<IDriver>>;

    SwContext(const path &local_storage_root_dir);
    virtual ~SwContext();

    void registerDriver(std::unique_ptr<IDriver> driver);
    const Drivers &getDrivers() const { return drivers; }
    SwBuild createBuild();

private:
    Drivers drivers;
};

} // namespace sw
