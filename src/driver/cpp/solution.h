// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "checks_storage.h"
#include "command.h"

#include <dependency.h>
#include <file_storage.h>
#include <target/base.h>

#include <sw/builder/driver.h>

#include <boost/bimap.hpp>
#include <boost/bimap/multiset_of.hpp>
#include <boost/dll/shared_library.hpp>
#include <boost/thread/shared_mutex.hpp>

namespace sw
{

struct Generator;
struct Module;

template <class T>
struct ExecutionPlan;

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

SW_DRIVER_CPP_API
String toString(FrontendType T);

struct SW_DRIVER_CPP_API Test : driver::cpp::CommandBuilder
{
    using driver::cpp::CommandBuilder::CommandBuilder;

    Test() = default;
    Test(const driver::cpp::CommandBuilder &cb)
        : driver::cpp::CommandBuilder(cb)
    {}

    void prepare(const Solution &s)
    {
        // todo?
    }
};

struct SW_DRIVER_CPP_API SolutionSettings
{
    OS TargetOS;
    NativeToolchain Native;

    // other langs?
    // make polymorphic?

    void init();
    String getConfig(const TargetBase *t, bool use_short_config = false) const;
};

struct Build;

/**
* \brief Single configuration solution.
*/
struct SW_DRIVER_CPP_API Solution : TargetBase
{
    using CommandExecutionPlan = ExecutionPlan<builder::Command>;

    // don't be so shy, don't hide in private
    OS HostOS;
    SolutionSettings Settings; // current configuration

    // solution (config) specific data
    mutable TargetMap TargetsToBuild;
    FileStorage *fs = nullptr;
    path fetch_dir;
    bool with_testing = false;
    String ide_solution_name;
    path config_file_or_dir; // original file or dir
    bool disable_compiler_lookup = false;
    path prefix_source_dir; // used for fetches (additional root dir to config/sources)
    const Build *build = nullptr;

    VariablesType Variables;

    // other data
    bool silent = false;

    // target data
    TargetMap children;
    TargetMap dummy_children; // contain dirs, projects,
    //TargetMap weak_children; // trash & garbage, used for storage internally, do not use

    SourceDirMap source_dirs_by_source;

    // for module calls
    String current_module;
    // for checks
    PackageVersionGroupNumber current_gn = 0;

    // for builder
    Checker checker;

    int execute_jobs = 0;

    // experimental
    // this sln will try to resolve from selected, so deps configs are changed
    //const Solution *resolve_from = nullptr;

    bool file_storage_local = true;
    int command_storage = 0;

public:
    Solution(const Solution &);
    //Solution &operator=(const Solution &);
    virtual ~Solution();

    TargetType getType() const override { return TargetType::Solution; }

    // impl
    void execute();
    void execute() const;
    void execute(CommandExecutionPlan &p) const;
    virtual void prepare(); // only checks use this
    virtual void performChecks();
    //void copyChecksFrom(const Solution &s);
    void clean() const;

    bool canRunTargetExecutables() const;
    void prepareForCustomToolchain();

    Commands getCommands() const;

    void printGraph(const path &p) const;

    // child targets
    TargetMap &getChildren() override;
    const TargetMap &getChildren() const override;
    bool exists(const PackageId &p) const override;

    //Package getKnownTarget(const PackagePath &ppath) const;
    bool isKnownTarget(const PackageId &p) const;

    // get solution dir for package
    path getSourceDir(const PackageId &p) const;
    std::optional<path> getSourceDir(const Source &s, const Version &v) const;
    path getIdeDir() const;
    path getExecutionPlansDir() const;
    path getExecutionPlanFilename() const;
    path getChecksDir() const;

    bool skipTarget(TargetScope Scope) const;

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

    using AvailableFrontends = boost::bimap<boost::bimaps::multiset_of<FrontendType>, path>;
    static const AvailableFrontends &getAvailableFrontends();
    static const std::set<FrontendType> &getAvailableFrontendTypes();
    static const StringSet &getAvailableFrontendNames();
    static const FilesOrdered &getAvailableFrontendConfigFilenames();
    static bool isFrontendConfigFilename(const path &fn);
    static std::optional<FrontendType> selectFrontendByFilename(const path &fn);

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

    // cross-compilation lies here
    void resolvePass(const Target &t, const DependenciesType &deps, const Solution *host) const;

    void addTest(Test &cb, const String &name);

    void setSettings();
    void findCompiler();

    virtual bool prepareStep();
    void prepareStep(Executor &e, Futures<void> &fs, std::atomic_bool &next_pass, const Solution *host) const;
    bool prepareStep(const TargetBaseTypePtr &t, const Solution *host) const;

private:
    friend struct ToBuild;
    friend struct Build;
};

/**
* \brief Main build class, controls solutions.
*/
struct SW_DRIVER_CPP_API Build : Solution, PackageScript
{
    struct FetchInfo
    {
        SourceDirMap sources;
    } fetch_info;

    std::optional<path> config; // current config or empty in configless mode
    //path current_dll; // current loaded dll
    // child solutions
    std::vector<Solution> solutions;
    Solution *current_solution = nullptr;
    bool configure = true;
    bool perform_checks = true;
    bool ide = false;

    Build();
    ~Build();

    TargetType getType() const override { return TargetType::Build; }

    path build(const path &fn);
    void load(const path &fn, bool configless = false);
    void build_package(const String &pkg);
    void build_packages(const StringSet &pkgs);
    void run_package(const String &pkg);
    bool execute() override;

    bool isConfigSelected(const String &s) const;
    const Module &loadModule(const path &fn) const;

    void prepare() override;
    bool prepareStep() override;

    Generator *getGenerator() { if (generator) return generator.get(); return nullptr; }
    const Generator *getGenerator() const { if (generator) return generator.get(); return nullptr; }

    CommandExecutionPlan getExecutionPlan() const override;

    // helper
    Solution &addSolutionRaw();
    Solution &addSolution();
    Solution &addCustomSolution();

    // hide?
    path build_configs(const std::unordered_set<ExtendedPackageData> &pkgs);

protected:
    PackageDescriptionMap getPackages() const override;

private:
    bool remove_ide_explans = false;
    std::optional<const Solution *> host;
    mutable StringSet used_configs;
    std::unique_ptr<Generator> generator;

    void setupSolutionName(const path &file_or_dir);
    SharedLibraryTarget &createTarget(const Files &files);
    path getOutputModuleName(const path &p);
    const Solution *getHostSolution();
    const Solution *getHostSolution() const;

    void performChecks() override;
    FilesMap build_configs_separate(const Files &files);

    void generateBuildSystem();

    // basic frontends
    void load_dll(const path &dll, bool usedll = true);
    void load_configless(const path &file_or_dir);
    void createSolutions(const path &dll, bool usedll = true);

    // other frontends
    void cppan_load();
    void cppan_load(const path &fn);
    void cppan_load(const yaml &root);
    bool cppan_check_config_root(const yaml &root);

public:
    static PackagePath getSelfTargetName(const Files &files);
};

}
