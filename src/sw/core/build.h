/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2020 Egor Pugin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "target.h"

#include <sw/builder/sw_context.h>
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

    // Tested?
};

// single build
struct SW_CORE_API SwBuild : SwBuilderContext
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
    void resolvePackages(); // [1/2] step
    void loadPackages();
    void prepare();
    void execute() const;

    // stop execution
    void stop();

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

    // tests
    void test();
    path getTestDir() const;

    //
    TargetMap &getTargets() { return targets; }
    const TargetMap &getTargets() const { return targets; }

    TargetMap &getTargetsToBuild() { return targets_to_build; }
    const TargetMap &getTargetsToBuild() const { return targets_to_build; }

    path getBuildDirectory() const;

    const std::vector<InputWithSettings> &getInputs() const;

    const TargetSettings &getExternalVariables() const;
    const TargetSettings &getSettings() const { return build_settings; }
    void setSettings(const TargetSettings &build_settings);

    void setName(const String &);
    String getName() const; // returns temporary object, so no refs

    void setEntryPoint(const PackageId &, const TargetEntryPointPtr &);
    void setServiceEntryPoint(const PackageId &, const TargetEntryPointPtr &);

private:
    SwContext &swctx;
    path build_dir;
    TargetMap targets;
    mutable TargetMap targets_to_build;
    std::vector<InputWithSettings> inputs;
    TargetSettings build_settings;
    mutable BuildState state = BuildState::NotStarted;
    mutable Commands commands_storage; // we need some place to keep copy cmds
    std::unique_ptr<Executor> build_executor;
    std::unique_ptr<Executor> prepare_executor;
    bool stopped = false;
    mutable ExecutionPlan *current_explan = nullptr;

    // other data
    String name;
    mutable FilesSorted fast_path_files;
    std::unordered_map<PackageId, TargetEntryPointPtr> entry_points;
    std::unordered_map<PackageId, TargetEntryPointPtr> service_entry_points;

    Commands getCommands() const;
    void loadPackages(const TargetMap &predefined);
    TargetEntryPointPtr getEntryPoint(const PackageId &) const;
    void resolvePackages(const std::vector<IDependency*> &upkgs); // [2/2] step
    Executor &getBuildExecutor() const;
    Executor &getPrepareExecutor() const;
};

} // namespace sw
