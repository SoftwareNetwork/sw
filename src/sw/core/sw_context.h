// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "target.h"

#include <sw/builder/os.h>
#include <sw/manager/sw_context.h>

namespace sw
{

struct IDriver;
struct Input;
struct InputDatabase;
struct SwBuild;
struct UserInput;

// core context for drivers
struct SW_CORE_API SwCoreContext : SwManagerContext
{
    SwCoreContext(const path &local_storage_root_dir, bool allow_network);
    virtual ~SwCoreContext();

    // from old builder ctx
    const OS &getHostOs() const { return HostOS; }

    const std::unordered_map<PackageId, TargetData> &getTargetData() const { return target_data; }
    TargetData &getTargetData(const PackageId &);
    const TargetData &getTargetData(const PackageId &) const;

    void setHostSettings(const PackageSettings &s);
    /// these host settings may be changed by the user
    const PackageSettings &getHostSettings() const { return host_settings; }
    /// original, unmodified host settings
    PackageSettings createHostSettings() const;

    InputDatabase &getInputDatabase();
    InputDatabase &getInputDatabase() const;

private:
    // rename to detected?
    // not only detected, but also predefined? do not rename?
    OS HostOS;
    std::unordered_map<PackageId, TargetData> target_data;
    PackageSettings host_settings;
    std::unique_ptr<InputDatabase> idb;
};

// public context
struct SW_CORE_API SwContext : SwCoreContext
{
    using Drivers = std::map<PackageId, std::unique_ptr<IDriver>>;

    SwContext(const path &local_storage_root_dir, bool allow_network);
    virtual ~SwContext();

    void registerDriver(const PackageId &pkg, std::unique_ptr<IDriver> driver);
    //const Drivers &getDrivers() const { return drivers; }

    std::unique_ptr<SwBuild> createBuild();
    void executeBuild(const path &);

    // stops current operation
    SwBuild *registerOperation(SwBuild &);
    void stop(std::thread::id);
    void stop();

    //
    std::vector<std::unique_ptr<Input>> detectInputs(const path &) const;
    static std::vector<std::unique_ptr<Input>> detectInputs(const std::vector<const IDriver*> &, const path &);
    Input *getInput(size_t hash) const;
    std::vector<Input *> addInputInternal(const path &);
    //                inserted
    std::pair<Input *, bool> registerInput(std::unique_ptr<Input>);
    Input *addInput(const Package &);
    // user inputs
    // select between package and path
    std::vector<UserInput> makeInput(const String &);
    // one subject may bring several inputs
    // (one path containing multiple inputs)
    std::vector<UserInput> makeInput(const path &, const PackagePath &prefix = {});
    // single input
    UserInput makeInput(const LocalPackage &);

    void loadEntryPointsBatch(const std::set<Input*> &inputs);

    const PackageSettings &getSettings() const { return settings; }
    void setSettings(const PackageSettings &s) { settings = s; }

private:
    using InputPtr = std::unique_ptr<Input>;
    using Inputs = std::map<size_t, InputPtr>;

    Drivers drivers;
    Inputs inputs;
    PackageSettings settings;
    std::mutex m;
    std::map<std::thread::id, SwBuild *> active_operations;
    bool stopped = false;

    std::unique_ptr<SwBuild> createBuild1();

};

} // namespace sw
