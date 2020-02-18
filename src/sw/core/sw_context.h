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
struct InputDatabase;
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

    void setHostSettings(const TargetSettings &s);
    /// these host settings may be changed by the user
    const TargetSettings &getHostSettings() const { return host_settings; }
    /// original, unmodified host settings
    TargetSettings createHostSettings() const;

    void setEntryPoint(const LocalPackage &, const TargetEntryPointPtr &);
    void setEntryPoint(const PackageId &, const TargetEntryPointPtr &);
    void setEntryPoint(PackageVersionGroupNumber, const TargetEntryPointPtr &);
    TargetEntryPointPtr getEntryPoint(const LocalPackage &) const;
    TargetEntryPointPtr getEntryPoint(const PackageId &) const;

private:
    // rename to detected?
    // not only detected, but also predefined? do not rename?
    TargetMap predefined_targets;
    std::unordered_map<PackageId, TargetData> target_data;
    TargetSettings host_settings;
    std::unordered_map<PackageId, TargetEntryPointPtr> entry_points;
    std::unordered_map<PackageVersionGroupNumber, TargetEntryPointPtr> entry_points_by_group_number;

    TargetEntryPointPtr getEntryPoint(PackageVersionGroupNumber) const;
};

// public context
struct SW_CORE_API SwContext : SwCoreContext
{
    using Drivers = std::map<PackageId, std::unique_ptr<IDriver>>;

    SwContext(const path &local_storage_root_dir);
    virtual ~SwContext();

    void registerDriver(const PackageId &pkg, std::unique_ptr<IDriver> &&driver);
    //const Drivers &getDrivers() const { return drivers; }

    std::unique_ptr<SwBuild> createBuild();
    void executeBuild(const path &);

    // one subject may bring several inputs
    // (one path containing multiple inputs)
    std::vector<Input *> addInput(const String &);
    std::vector<Input *> addInput(const LocalPackage &);
    std::vector<Input *> addInput(const path &);
    //                inserted
    std::pair<Input *, bool> registerInput(std::unique_ptr<Input>);

    void loadEntryPointsBatch(const std::set<Input*> &inputs);

private:
    using InputPtr = std::unique_ptr<Input>;
    using Inputs = std::map<size_t, InputPtr>;

    Drivers drivers;
    Inputs inputs;
    std::unique_ptr<InputDatabase> idb;

    std::unique_ptr<SwBuild> createBuild1();
};

} // namespace sw
