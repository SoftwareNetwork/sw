// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "input.h"
#include "target.h"

#include <sw/builder/execution_plan.h>
#include <sw/manager/package_data.h>

namespace sw
{

struct IDriver;
struct Input;
struct SwContext;

// single build
struct SwBuild
{
    using CommandExecutionPlan = ExecutionPlan<builder::Command>;

    const SwContext &swctx;

    SwBuild(const SwContext &swctx);
    //SwBuild(const SwBuild &) = default;
    //SwBuild &operator=(const SwBuild &) = default;

    Input &addInput(const String &);
    Input &addInput(const path &);
    Input &addInput(const PackageId &);

    // complete
    void build();

    // precise
    void load();
    void setTargetsToBuild();
    void resolvePackages();
    void prepare();
    void execute();

    // tune
    bool prepareStep();
    void execute(CommandExecutionPlan &p) const;
    CommandExecutionPlan getExecutionPlan(const Commands &cmds) const;

    CommandExecutionPlan getExecutionPlan() const;
    String getSpecification() const;

    String getHash() const;

    TargetMap &getTargets() { return targets; }
    const TargetMap &getTargets() const { return targets; }

    TargetMap &getTargetsToBuild() { return targets_to_build; }
    const TargetMap &getTargetsToBuild() const { return targets_to_build; }

private:
    using ProcessedInputs = std::set<Input>;

    ProcessedInputs inputs;
    TargetMap targets;
    TargetMap targets_to_build;

    void load(ProcessedInputs &inputs);
    Commands getCommands() const;
};

} // namespace sw
