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

// represents user request (if possible) returned from sw context
// or sw context = single request?
// or ... ?
//struct SW_CORE_API Request {};

namespace detail
{

struct InputVariant : std::variant<String, path, PackageId>
{
    using Base = std::variant<String, path, PackageId>;
    using Base::Base;
    InputVariant(const char *p) : Base(std::string(p)) {}
};

// unique
struct Inputs : std::set<InputVariant>
{
    using Base = std::set<InputVariant>;
    using Base::Base; // separate types
    Inputs(const Strings &inputs) // dynamic detection
    {
        for (auto &i : inputs)
            insert(i);
    }
};

}

struct SW_CORE_API SwContext : SwBuilderContext
{
    using CommandExecutionPlan = ExecutionPlan<builder::Command>;
    using Drivers = std::map<PackageId, std::unique_ptr<IDriver>>;
    using Inputs = detail::Inputs;

    // move to drivers? remove?
    path source_dir;

    SwContext(const path &local_storage_root_dir);
    virtual ~SwContext();

    void registerDriver(std::unique_ptr<IDriver> driver);
    const Drivers &getDrivers() const { return drivers; }

    // TODO: return some build object? why? swctx IS buildobj, isn't it?
    //std::unique_ptr<Request> load(const Inputs &inputs);
    void load(const Inputs &inputs);
    void build(const Inputs &inputs);
    void execute();
    void configure();
    CommandExecutionPlan getExecutionPlan() const;
    // void configure(); // = load() + save execution plan
    PackageDescriptionMap getPackages() const;
    String getBuildHash() const;

    TargetMap &getTargets() { return targets; }
    const TargetMap &getTargets() const { return targets; }
    const TargetMap &getTargetsToBuild() const { return targets_to_build; }

private:
    using ProcessedInputs = std::set<Input>;

    Drivers drivers;
    std::map<IDriver *, ProcessedInputs> active_drivers;
    ProcessedInputs inputs;
    TargetMap targets;
    TargetMap targets_to_build;

    ProcessedInputs makeInputs(const Inputs &inputs);
    void load(const ProcessedInputs &inputs);
    bool prepareStep();
    void resolvePackages();
    void execute(CommandExecutionPlan &p) const;
    CommandExecutionPlan getExecutionPlan(const Commands &cmds) const;
    Commands getCommands() const;
};

struct Input
{
    Input(const String &, const SwContext &);
    Input(const path &, const SwContext &);
    Input(const PackageId &, const SwContext &);

    IDriver &getDriver() const { return *driver; }
    InputType getType() const { return type; }
    path getPath() const;
    PackageId getPackageId() const;
    bool isChanged() const;
    const TargetSettings &getSettings() const { return settings; }
    String getHash() const;

    bool operator<(const Input &rhs) const;

private:
    std::variant<path, PackageId> data;
    InputType type;
    IDriver *driver = nullptr;
    TargetSettings settings;

    void init(const String &, const SwContext &);
    void init(const path &, const SwContext &);
    void init(const PackageId &, const SwContext &);
};

} // namespace sw
