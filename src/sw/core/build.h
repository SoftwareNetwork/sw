// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "target.h"

#include <sw/manager/package_data.h>

namespace sw
{

struct ExecutionPlan;
struct Input;
struct InputWithSettings;
struct SwContext;

enum class BuildState
{
    NotStarted,

    InputsLoaded,
    TargetsToBuildSet,
    PackagesResolved,
    PackagesLoaded,
    Prepared,
    Executed,
};

// single build
struct SW_CORE_API SwBuild
{
    SwBuild(SwContext &swctx, const path &build_dir);
    SwBuild(const SwBuild &) = delete;
    SwBuild &operator=(const SwBuild &) = delete;
    ~SwBuild();

    SwContext &getContext() { return swctx; }
    const SwContext &getContext() const { return swctx; }

    void addInput(const InputWithSettings &);

    // complete
    void build();

    // precise
    void loadInputs();
    void setTargetsToBuild();
    void resolvePackages();
    void loadPackages();
    void prepare();
    void execute() const;

    // tune
    bool prepareStep();
    void execute(ExecutionPlan &p) const;
    ExecutionPlan getExecutionPlan(const Commands &cmds) const;
    bool step();
    void overrideBuildState(BuildState) const;
    // explans
    void saveExecutionPlan() const;
    void runSavedExecutionPlan() const;
    void saveExecutionPlan(const path &) const;
    void runSavedExecutionPlan(const path &) const;
    ExecutionPlan getExecutionPlan() const;
    String getHash() const;
    path getExecutionPlanPath() const;

    TargetMap &getTargets() { return targets; }
    const TargetMap &getTargets() const { return targets; }

    TargetMap &getTargetsToBuild() { return targets_to_build; }
    const TargetMap &getTargetsToBuild() const { return targets_to_build; }

    path getBuildDirectory() const;

    // install packages and add them to build
    std::unordered_map<UnresolvedPackage, LocalPackage> install(const UnresolvedPackages &pkgs);
    const PackageIdSet &getKnownPackages() const;
    void addKnownPackage(const PackageId &);

    std::vector<InputWithSettings> getInputs() const;

private:
    SwContext &swctx;
    path build_dir;
    TargetMap targets;
    TargetMap targets_to_build;
    PackageIdSet known_packages;
    std::vector<InputWithSettings> inputs;
    mutable BuildState state = BuildState::NotStarted;

    void load(const std::vector<InputWithSettings> &inputs, bool set_eps);
    Commands getCommands() const;
    void loadPackages(const TargetMap &predefined);
};

} // namespace sw
