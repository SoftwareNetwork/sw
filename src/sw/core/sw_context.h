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
struct Input;
struct SwBuild;

// core context for drivers
struct SW_CORE_API SwCoreContext : SwBuilderContext
{
    SwCoreContext(const path &local_storage_root_dir);
    virtual ~SwCoreContext();

    TargetMap &getPredefinedTargets() { return predefined_targets; }
    const TargetMap &getPredefinedTargets() const { return predefined_targets; }

    const std::unordered_map<PackageId, TargetData> &getTargetData() const { return target_data; }
    TargetData &getTargetData(const PackageId &);
    const TargetData &getTargetData(const PackageId &) const;

    void setHostSettings(const TargetSettings &s) { host_settings = s; }
    const TargetSettings &getHostSettings() const { return host_settings; }

private:
    // rename to detected?
    // not only detected, but also predefined? do not rename?
    TargetMap predefined_targets;
    std::unordered_map<PackageId, TargetData> target_data;
    TargetSettings host_settings;

    void createHostSettings();
    void setHostPrograms();
};

// public context
struct SW_CORE_API SwContext : SwCoreContext
{
    using Drivers = std::map<PackageId, std::unique_ptr<IDriver>>;

    SwContext(const path &local_storage_root_dir);
    virtual ~SwContext();

    void registerDriver(const PackageId &pkg, std::unique_ptr<IDriver> &&driver);
    const Drivers &getDrivers() const { return drivers; }

    std::unique_ptr<SwBuild> createBuild();
    void executeBuild(const path &);

    Input &addInput(const String &);
    Input &addInput(const path &);
    Input &addInput(const LocalPackage &);

    void loadEntryPoints(const std::set<Input*> &inputs, bool set_eps);

private:
    using InputPtr = std::unique_ptr<Input>;
    using Inputs = std::vector<InputPtr>;

    Drivers drivers;
    Inputs inputs;

    std::unique_ptr<SwBuild> createBuild1();
    template <class I>
    Input &addInput1(const I &);
};

} // namespace sw
