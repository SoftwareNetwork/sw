// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "driver.h"
#include "target.h"

#include <sw/builder/execution_plan.h>
#include <sw/builder/sw_context.h>
#include <sw/manager/package_data.h>

namespace sw
{

enum class InputType : int32_t
{
    Unspecified = 0,

    /// drivers may use their own methods for better loading packages
    /// rather than when direct spec file provided
    InstalledPackage,

    ///
    SpecificationFile,

    /// from some regular file
    InlineSpecification,

    /// only try to find spec file
    DirectorySpecificationFile,

    /// no input file, use any heuristics
    Directory,
};

struct SW_CORE_API SwContext : SwBuilderContext
{
    using CommandExecutionPlan = ExecutionPlan<builder::Command>;
    using Drivers = std::map<PackageId, std::unique_ptr<IDriver>>;

    // move to drivers? remove?
    path source_dir;

    SwContext(const path &local_storage_root_dir);
    virtual ~SwContext();

    void registerDriver(std::unique_ptr<IDriver> driver);
    const Drivers &getDrivers() const { return drivers; }

    Input &addInput(const String &);
    Input &addInput(const path &);
    Input &addInput(const PackageId &);

    void load();
    void build();
    void execute();
    void configure();

    CommandExecutionPlan getExecutionPlan() const;
    PackageDescriptionMap getPackages() const;

    String getBuildHash() const;

    TargetMap &getTargets() { return targets; }
    const TargetMap &getTargets() const { return targets; }
    const TargetMap &getTargetsToBuild() const { return targets_to_build; }

private:
    using ProcessedInputs = std::set<Input>;

    Drivers drivers;
    ProcessedInputs inputs;
    TargetMap targets;
    TargetMap targets_to_build;

    void load(ProcessedInputs &inputs);
    bool prepareStep();
    void resolvePackages();
    void execute(CommandExecutionPlan &p) const;
    CommandExecutionPlan getExecutionPlan(const Commands &cmds) const;
    Commands getCommands() const;
};

struct Input
{
    Input(const path &, const SwContext &);
    Input(const PackageId &, const SwContext &);

    IDriver &getDriver() const { return *driver; }
    InputType getType() const { return type; }
    path getPath() const;
    PackageId getPackageId() const;
    bool isChanged() const;
    const std::set<TargetSettings> &getSettings() const;
    void addSettings(const TargetSettings &s);
    void clearSettings() { settings.clear(); }
    String getHash() const;

    bool operator<(const Input &rhs) const;

private:
    std::variant<path, PackageId> data;
    InputType type;
    IDriver *driver = nullptr;
    std::set<TargetSettings> settings;

    void init(const path &, const SwContext &);
    void init(const PackageId &, const SwContext &);
};

} // namespace sw
