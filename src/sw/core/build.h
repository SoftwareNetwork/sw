// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "target.h"

#include <sw/builder/sw_context.h>

namespace sw
{

struct ExecutionPlan;
struct Input;
struct UserInput;
struct SwContext;
struct ResolveRequest;
struct CachedStorage;
struct CachingResolver;

enum class BuildState
{
    NotStarted,

    InputsLoaded,
    PackagesResolved,
    PackagesLoaded,
    Prepared,
    Executed,

    // Tested?
};

// single build
struct SW_CORE_API SwBuild : SwBuilderContext, ResolverHolder
{
    SwBuild(SwContext &swctx, const path &build_dir);
    SwBuild(const SwBuild &) = delete;
    SwBuild &operator=(const SwBuild &) = delete;
    //SwBuild(SwBuild &&) = default;
    ~SwBuild();

    SwContext &getContext() { return swctx; }
    const SwContext &getContext() const { return swctx; }

    // add user inputs
    void addInput(const UserInput &);
    LogicalInput &getInput(Input &);

    // complete
    void build();

    // precise
    void loadInputs();
    //void setTargetsToBuild();
    void resolvePackages(); // [1/2] step
private:
    void loadPackages();
public:
    void prepare();
    void execute() const;

    // stop execution
    void stop();

    // tune
    bool prepareStep();
    void execute(ExecutionPlan &p) const;
    std::unique_ptr<ExecutionPlan> getExecutionPlan(const Commands &cmds) const;
    bool step();
    void overrideBuildState(BuildState) const;
    // explans
    void saveExecutionPlan() const;
    void runSavedExecutionPlan() const;
    void saveExecutionPlan(const path &) const;
    void runSavedExecutionPlan(const path &) const;
    std::unique_ptr<ExecutionPlan> getExecutionPlan() const;
    String getHash() const;
    path getExecutionPlanPath() const;

    // tests
    void test();
    path getTestDir() const;

    //
    TargetMap &getTargets() { return targets; }
    const TargetMap &getTargets() const { return targets; }

    //TargetMap &getTargetsToBuild() { return targets_to_build; }
    //const TargetMap &getTargetsToBuild() const { return targets_to_build; }

    path getBuildDirectory() const;

    const std::vector<UserInput> &getInputs() const;

    const PackageSettings &getExternalVariables() const;
    const PackageSettings &getSettings() const { return build_settings; }
    void setSettings(const PackageSettings &build_settings);

    void setName(const String &);
    String getName() const; // returns temporary object, so no refs

    bool isPredefinedTarget(const PackagePath &) const;
    template <class T>
    bool isPredefinedTarget(const T &p) const
    {
        return isPredefinedTarget(p.getPath());
    }

    // stable resolve during whole build
    //bool resolve(ResolveRequest &) const;

    using RegisterTargetsResult = std::vector<ITarget *>;
    ITarget *registerTarget(ITargetPtr);
    RegisterTargetsResult registerTargets(std::vector<ITargetPtr> &);

private:
    SwContext &swctx;
    path build_dir;
    TargetMap targets;
    //mutable TargetMap targets_to_build;
    std::vector<UserInput> user_inputs;
    PackageSettings build_settings;
    mutable BuildState state = BuildState::NotStarted;
    mutable Commands commands_storage; // we need some place to keep copy cmds
    std::unique_ptr<Executor> build_executor;
    std::unique_ptr<Executor> prepare_executor;
    bool stopped = false;
    mutable ExecutionPlan *current_explan = nullptr;
    std::unique_ptr<CachedStorage> cached_storage;
    std::unique_ptr<CachingResolver> cr;
    std::vector<ITargetPtr> target_storage;
    //std::set<ITarget*> build_targets;
    using MapKey = Input *;
    std::unordered_map<PackageId, MapKey> logical_inputs;
    std::unordered_map<MapKey, LogicalInput> input_storage;

    // other data
    String name;
    mutable FilesSorted fast_path_files;

    Commands getCommands() const;
    void resolvePackages(const std::vector<IDependency*> &upkgs); // [2/2] step
    Executor &getBuildExecutor() const;
    Executor &getPrepareExecutor() const;
    LogicalInput addInput(const Package &);
    LogicalInput &addInput(LogicalInput &&);

    void resolveWithDependencies(std::vector<ResolveRequest> &) const;
};

} // namespace sw
