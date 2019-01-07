// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <dependency.h>
#include "checks_storage.h"

#include <file_storage.h>
#include <execution_plan.h>
#include <target.h>

#include <sw/builder/driver.h>

#include <boost/bimap.hpp>
#include <boost/dll/shared_library.hpp>
#include <boost/thread/shared_mutex.hpp>

namespace sw
{

namespace detail
{

struct EventCallback
{
    using BasicEventCallback = std::function<void(TargetBase &t, CallbackType e)>;
    using TypedEventCallback = std::function<void(TargetBase &t)>;

    PackagesIdSet pkgs;
    std::set<CallbackType> types;
    BasicEventCallback cb;
    bool typed_cb = false;

    void operator()(TargetBase &t, CallbackType e);

    template <class F, class ... Args>
    void add(const F &a, Args &&... args)
    {
        if constexpr (std::is_same_v<F, BasicEventCallback> ||
            std::is_convertible_v<F, BasicEventCallback>)
            cb = a;
        else if constexpr (std::is_same_v<F, TypedEventCallback> ||
            std::is_convertible_v<F, TypedEventCallback>)
        {
            typed_cb = true;
            cb = [a](TargetBase &t, CallbackType)
            {
                a(t);
            };
        }
        else if constexpr (std::is_same_v<F, CallbackType>)
            types.insert(a);
        else
            pkgs.insert(String(a));

        if constexpr (sizeof...(Args) > 0)
            add(std::forward<Args>(args)...);
    }
};

}

using FilesMap = std::unordered_map<path, path>;

enum class FrontendType
{
    // priority!
    Sw = 1,
    Cppan = 2,
};

struct SW_DRIVER_CPP_API Test : driver::cpp::CommandBuilder
{
    using driver::cpp::CommandBuilder::CommandBuilder;

    Test() = default;
    Test(const driver::cpp::CommandBuilder &cb)
        : driver::cpp::CommandBuilder(cb)
    {}
};

/**
* \brief Single configuration solution.
*/
struct SW_DRIVER_CPP_API Solution : TargetBase
{
    using CommandExecutionPlan = ExecutionPlan<builder::Command>;

    // solution (config) specific data
    mutable TargetMap TargetsToBuild;
    FileStorage *fs = nullptr;
    path fetch_dir;
    bool with_testing = false;
    String ide_solution_name;
    path config_file_or_dir; // original file or dir

    VariablesType Variables;

    // other data
    bool silent = false;

    // target data
    TargetMap children;
    TargetMap dummy_children; // contain dirs, projects,
    TargetMap weak_children; // trash & garbage, used for storage internally, do not use

    SourceDirMap source_dirs_by_source;

    // for module calls
    String current_module;
    // for checks
    PackageVersionGroupNumber current_gn = 0;

    // for builder
    Checker checker;

public:
    Solution(const Solution &);
    //Solution &operator=(const Solution &);
    virtual ~Solution();

    TargetType getType() const override { return TargetType::Solution; }

    // impl
    void execute();
    void execute() const;
    void execute(CommandExecutionPlan &p) const;
    virtual void prepare();
    bool prepareStep();
    virtual void performChecks();
    //void copyChecksFrom(const Solution &s);
    void clean() const;

    Commands getCommands() const;

    void printGraph(const path &p) const;

    // helper
    virtual Solution &addSolution() { throw std::logic_error("invalid call"); }

    // child targets
    TargetMap &getChildren() override;
    const TargetMap &getChildren() const override;
    bool exists(const PackageId &p) const override;

    //Package getKnownTarget(const PackagePath &ppath) const;
    bool isKnownTarget(const PackageId &p) const;

    // get solution dir for package
    path getSourceDir(const PackageId &p) const;
    optional<path> getSourceDir(const Source &s, const Version &v) const;
    path getIdeDir() const;
    path getExecutionPlansDir() const;
    path getExecutionPlanFilename() const;
    path getChecksDir() const;

    bool skipTarget(TargetScope Scope) const;

    TargetBaseTypePtr resolveTarget(const UnresolvedPackage &) const;

    // tests
    // TODO: implement some of https://cmake.org/cmake/help/latest/manual/cmake-properties.7.html#properties-on-tests
    Commands tests;
    Test addTest(const ExecutableTarget &t);
    Test addTest(const String &name, const ExecutableTarget &t);
    Test addTest();
    Test addTest(const String &name);
    path getTestDir() const;

    //protected:
    // known targets are downloaded
    // unknown targets have PostponeFileResolving set
    PackagesIdSet knownTargets;

    virtual CommandExecutionPlan getExecutionPlan() const;
    CommandExecutionPlan getExecutionPlan(const Commands &cmds) const;

    // events
    template <class ... Args>
    void registerCallback(Args &&... args)
    {
        static_assert(sizeof...(Args) != 0, "Missing callback");

        detail::EventCallback c;
        c.add(std::forward<Args>(args)...);
        events.push_back(c);
    }
    void call_event(TargetBase &t, CallbackType et);
    //

    static const boost::bimap<FrontendType, path> &getAvailableFrontends();
    static const FilesOrdered &getAvailableFrontendConfigFilenames();
    static bool isFrontendConfigFilename(const path &fn);
    static optional<FrontendType> selectFrontendByFilename(const path &fn);

protected:
    Solution &base_ptr;
    bool dry_run = false;

    Solution();
    void clear();

private:
    std::unordered_set<ExtendedPackageData> known_cfgs;
    std::vector<detail::EventCallback> events;
    //Files used_modules;
    mutable std::unordered_map<UnresolvedPackage, TargetBaseTypePtr> resolved_targets;

    //void checkPrepared() const;
    UnresolvedDependenciesType gatherUnresolvedDependencies() const;
    void build_and_resolve(int n_runs = 0);

    path getChecksFilename() const;

    void addTest(Test &cb, const String &name);

private:
    friend struct ToBuild;
};

struct SW_DRIVER_CPP_API Build : Solution, PackageScript
{
    struct FetchInfo
    {
        SourceDirMap sources;
    } fetch_info;

    optional<path> config; // current config or empty in configless mode
    path dll; // current loaded dll
    // child solutions
    std::vector<Solution> solutions;
    Solution *current_solution = nullptr;
    bool configure = false;
    bool perform_checks = true;
    bool ide = false;

    Build();
    ~Build();

    TargetType getType() const override { return TargetType::Build; }

    path build_configs(const std::unordered_set<ExtendedPackageData> &pkgs);
    FilesMap build_configs_separate(const Files &files);

    path build(const path &fn);
    void build_and_load(const path &fn);
    void build_and_run(const path &fn);
    void build_package(const String &pkg);
    void run_package(const String &pkg);
    void load(const path &dll, bool usedll = true);
    bool execute() override;
    bool load_configless(const path &file_or_dir);

    void performChecks() override;
    void prepare() override;

    bool generateBuildSystem();
    CommandExecutionPlan getExecutionPlan() const override;

    // helper
    Solution &addSolution() override;

    // other frontends
    void cppan_load();
    void cppan_load(const path &fn);
    void cppan_load(const yaml &root);
    bool cppan_check_config_root(const yaml &root);

protected:
    PackageDescriptionMap getPackages() const;

private:
    bool remove_ide_explans = false;

    void setSettings();
    void findCompiler();
    void setupSolutionName(const path &file_or_dir);
    SharedLibraryTarget &createTarget(const Files &files);

public:
    static PackagePath getSelfTargetName(const Files &files);
};

}
