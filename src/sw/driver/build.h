// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "build_settings.h"
#include "checks_storage.h"
#include "command.h"
#include "target/base.h"

#include <sw/builder/file_storage.h>
#include <sw/core/target.h>
#include <sw/manager/package_data.h>

#include <boost/bimap.hpp>
#include <boost/bimap/multiset_of.hpp>
#include <boost/dll/shared_library.hpp>
#include <boost/thread/shared_mutex.hpp>

#include <any>
#include <variant>

namespace sw
{

struct Build;
namespace driver::cpp { struct Driver; }
struct Generator;
struct Module;
struct ModuleStorage;

template <class T>
struct ExecutionPlan;

namespace detail
{

struct EventCallback
{
    using BasicEventCallback = std::function<void(Target &t, CallbackType e)>;
    using TypedEventCallback = std::function<void(Target &t)>;

    PackageIdSet pkgs;
    std::set<CallbackType> types;
    BasicEventCallback cb;
    bool typed_cb = false;

    void operator()(Target &t, CallbackType e);

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
            cb = [a](Target &t, CallbackType)
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

struct SW_DRIVER_CPP_API Test : driver::CommandBuilder
{
    using driver::CommandBuilder::CommandBuilder;

    Test() = default;
    Test(const driver::CommandBuilder &cb)
        : driver::CommandBuilder(cb)
    {}

    void prepare(const Build &s)
    {
        // todo?
    }
};

struct ModuleSwappableData
{
    //std::weak_ptr<TargetEntryPoint> ep;
    PackagePath NamePrefix;
    String current_module;
    PackageVersionGroupNumber current_gn = 0;
    TargetSettings current_settings;
    BuildSettings bs;
    PackageIdSet known_targets;
    std::vector<Target*> added_targets;
};

// this driver ep
struct SW_DRIVER_CPP_API NativeTargetEntryPoint : TargetLoader,
    std::enable_shared_from_this<NativeTargetEntryPoint>
{
    ModuleSwappableData module_data;

    NativeTargetEntryPoint(Build &b);

    void loadPackages(const TargetSettings &, const PackageIdSet &pkgs = {}) override;
    void addChild(const TargetBaseTypePtr &t);

protected:
    Build &b;

private:
    virtual void loadPackages1() = 0;
};

struct NativeBuiltinTargetEntryPoint : NativeTargetEntryPoint
{
    using BuildFunction = void(*)(Build &);
    using CheckFunction = void(*)(Checker &);

    BuildFunction bf = nullptr;
    CheckFunction cf = nullptr;

    NativeBuiltinTargetEntryPoint(Build &b, BuildFunction bf);

private:
    void loadPackages1() override;
};

struct SimpleBuild : TargetBase
{
};

struct SW_DRIVER_CPP_API Build : SimpleBuild
{
    using Base = SimpleBuild;

    // rename to preferences?
    /*using TargetSettingsDataContainer = std::any; // or void*?
    struct TargetSettingsData
    {
        std::regex r_ppath; // compiled and cached regex
        VersionRange range;
        // add deps, features? (target internal settings)
        TargetSettingsDataContainer data;
    };*/

    using CommandExecutionPlan = ExecutionPlan<builder::Command>;

    // most important
    SwContext &swctx;
    const driver::cpp::Driver &driver;
    std::vector<BuildSettings> settings; // initial settings
private:
    TargetSettings host_settings;
public:
    //std::map<String, std::vector<TargetSettingsData>> target_settings; // regex, some data

    //
    std::vector<TargetBaseTypePtr> dummy_children;
    int command_storage = 0;
    const ModuleSwappableData *module_data = nullptr;
    std::weak_ptr<NativeTargetEntryPoint> ntep;
    SourceDirMap source_dirs_by_source;
    int execute_jobs = 0;
    bool file_storage_local = true;
    bool is_config_build = false;
    path config_file_or_dir; // original file or dir
    String ide_solution_name;
    bool disable_compiler_lookup = false;
    Checker checker;
    mutable TargetMap TargetsToBuild;
    // other data
    bool silent = false; // some log messages
    bool show_output = false; // output from commands
    path fetch_dir;
    bool dry_run = false;
    bool with_testing = false;
    std::unordered_set<LocalPackage> known_cfgs;
    bool use_separate_target_map = false; // check targets added to internal children map
private:
    TargetMap internal_targets;
public:

    const OS &getHostOs() const;
    const TargetSettings &getHostSettings() const;
    const BuildSettings &getBuildSettings() const;
    const TargetSettings &getSettings() const;
    bool isKnownTarget(const LocalPackage &p) const;
    path getSourceDir(const LocalPackage &p) const;
    std::optional<path> getSourceDir(const Source &s, const Version &v) const;
    bool skipTarget(TargetScope Scope) const;
    //bool exists(const PackageId &p) const;
    TargetMap &getChildren();
    const TargetMap &getChildren() const;
    path getChecksDir() const;
    path getIdeDir() const;
    path getExecutionPlansDir() const;
    path getExecutionPlanFilename() const;
    CommandExecutionPlan getExecutionPlan() const;
    CommandExecutionPlan getExecutionPlan(const Commands &cmds) const;
    Commands getCommands() const;
    void addFirstConfig();
    void detectCompilers();
    PackageDescriptionMap getPackages() const;
    BuildSettings createSettings() const;
    void addSettings(const BuildSettings &);
    path build_configs(const std::unordered_set<LocalPackage> &pkgs);
    const ModuleSwappableData &getModuleData() const;
    PackageVersionGroupNumber getCurrentGroupNumber() const;
    const String &getCurrentModule() const;
    //std::shared_ptr<TargetEntryPoint> getEntryPoint() const;
    void addChild(const TargetBaseTypePtr &t);

    /*void addTargetSettings(const String &ppath_regex, const VersionRange &vr, const TargetSettingsDataContainer &);
    template <class T>
    std::vector<const T *> getTargetSettings(const PackageId &id) const
    {
        auto s = id.ppath.toString();
        std::vector<const T *> d;
        for (auto &[k, v] : target_settings)
        {
            for (auto &ts : v)
            {
                if (!std::regex_match(s, ts.r_ppath))
                    continue;
                if (!ts.range.hasVersion(id.version))
                    continue;
                if (!ts.data.has_value())
                    continue;
                if (auto d2 = std::any_cast<T>(&ts.data))
                    d.push_back(d2);
            }
        }
        return d;
    }*/

    // events
    template <class ... Args>
    void registerCallback(Args &&... args)
    {
        static_assert(sizeof...(Args) != 0, "Missing callback");

        detail::EventCallback c;
        c.add(std::forward<Args>(args)...);
        events.push_back(c);
    }
    void call_event(Target &t, CallbackType et);
    //

    // tests
    // TODO: implement some of https://cmake.org/cmake/help/latest/manual/cmake-properties.7.html#properties-on-tests
    Commands tests;
    Test addTest(const ExecutableTarget &t);
    Test addTest(const String &name, const ExecutableTarget &t);
    Test addTest();
    Test addTest(const String &name);
    path getTestDir() const;

    using AvailableFrontends = boost::bimap<boost::bimaps::multiset_of<FrontendType>, path>;
    static const AvailableFrontends &getAvailableFrontends();
    static const std::set<FrontendType> &getAvailableFrontendTypes();
    static const StringSet &getAvailableFrontendNames();
    static const FilesOrdered &getAvailableFrontendConfigFilenames();
    static bool isFrontendConfigFilename(const path &fn);
    static std::optional<FrontendType> selectFrontendByFilename(const path &fn);

    void build_self();
    FilesMap build_configs_separate(const Files &files);

private:
    void resolvePass(const Target &t, const DependenciesType &deps) const;
    void prepareStep(Executor &e, Futures<void> &fs, std::atomic_bool &next_pass) const;
    bool prepareStep(Target &t) const;
    UnresolvedDependenciesType gatherUnresolvedDependencies(int n_runs = 0);
    void build_and_resolve(int n_runs = 0);
    void addTest(Test &cb, const String &name);

public:

    //




















    struct FetchInfo
    {
        SourceDirMap sources;
    } fetch_info;

    std::optional<path> config; // current config or empty in configless mode
    // child solutions
    //std::vector<Solution> solutions;
    //Solution *current_solution = nullptr;
    bool configure = true;
    bool perform_checks = true;
    bool ide = false;

    Build(SwContext &swctx, const driver::cpp::Driver &driver);
    Build(const Build &);
    ~Build();

    void load_spec_file(const path &);
    void load_inline_spec(const path &);
    void load_dir(const path &);

    path build(const path &fn);
    void load(const path &fn, bool configless = false);
    void load_packages(const StringSet &pkgs);
    void build_packages(const StringSet &pkgs);
    void run_package(const String &pkg);
    void execute();
    void execute(CommandExecutionPlan &p) const;

    bool isConfigSelected(const String &s) const;
    Module loadModule(const path &fn) const;

    void prepare();
    bool prepareStep();

    Generator *getGenerator() { if (generator) return generator.get(); return nullptr; }
    const Generator *getGenerator() const { if (generator) return generator.get(); return nullptr; }

    // helper
    //Solution &addSolutionRaw();
    //Solution &addSolution();
    //Solution &addCustomSolution();

private:
    bool remove_ide_explans = false;
    //std::optional<const Solution *> host;
    mutable StringSet used_configs;
    std::shared_ptr<Generator> generator; // not unique - just allow us to copy builds
    bool solutions_created = false;
    std::vector<detail::EventCallback> events;

    //std::optional<std::reference_wrapper<Solution>> addFirstSolution();
    void setupSolutionName(const path &file_or_dir);
    SharedLibraryTarget &createTarget(const Files &files);
    //const Solution *getHostSolution();
    //const Solution *getHostSolution() const;

    void generateBuildSystem();

    // basic frontends
    void load_dll(const path &dll);
    void load_configless(const path &file_or_dir);
    void createSolutions(const path &dll, bool usedll = true);

    // other frontends
    void cppan_load();
    void cppan_load(const path &fn);
    void cppan_load(const yaml &root, const String &root_name = {});
    bool cppan_check_config_root(const yaml &root);
};

}
