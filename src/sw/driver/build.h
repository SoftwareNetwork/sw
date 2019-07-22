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
#include <sw/core/build.h>
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
struct Module;
struct ModuleStorage;
struct SwContext;

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

struct NativeTargetEntryPoint;

struct ModuleSwappableData
{
    std::weak_ptr<NativeTargetEntryPoint> ntep;
    PackagePath NamePrefix;
    String current_module;
    PackageVersionGroupNumber current_gn = 0;
    TargetSettings current_settings;
    BuildSettings bs;
    PackageIdSet known_targets;
    std::vector<Target*> added_targets;
};

struct PrepareConfigEntryPoint;

// this driver ep
struct SW_DRIVER_CPP_API NativeTargetEntryPoint : TargetEntryPoint,
    std::enable_shared_from_this<NativeTargetEntryPoint>
{
    ModuleSwappableData module_data;

    NativeTargetEntryPoint(Build &b);

    void loadPackages(const TargetSettings &, const PackageIdSet &pkgs) override;
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

    using CommandExecutionPlan = ExecutionPlan<builder::Command>;

    // most important
    const SwContext &swctx;
    SwBuild &main_build;
    const driver::cpp::Driver &driver;
private:
    TargetSettings host_settings;
public:

    //
    std::vector<TargetBaseTypePtr> dummy_children;
    int command_storage = 0;
    const ModuleSwappableData *module_data = nullptr;
    SourceDirMap source_dirs_by_source;
    int execute_jobs = 0;
    bool is_config_build = false;
    Checker checker;
    mutable TargetMap TargetsToBuild;
    // other data
    bool silent = false; // some log messages
    bool show_output = false; // output from commands
    path fetch_dir;
    std::unordered_set<LocalPackage> known_cfgs;
    bool use_separate_target_map = false; // check targets added to internal children map
private:
    SwBuild b;
public:

    const OS &getHostOs() const;
    const TargetSettings &getHostSettings() const;
    const BuildSettings &getBuildSettings() const;
    const TargetSettings &getSettings() const;
    bool isKnownTarget(const LocalPackage &p) const;
    path getSourceDir(const LocalPackage &p) const;
    std::optional<path> getSourceDir(const Source &s, const Version &v) const;
    bool skipTarget(TargetScope Scope) const;
    TargetMap &getChildren();
    const TargetMap &getChildren() const;
    path getChecksDir() const;
    CommandExecutionPlan getExecutionPlan() const;
    CommandExecutionPlan getExecutionPlan(const Commands &cmds) const;
    Commands getCommands() const;
    const ModuleSwappableData &getModuleData() const;
    PackageVersionGroupNumber getCurrentGroupNumber() const;
    const String &getCurrentModule() const;
    void addChild(const TargetBaseTypePtr &t);

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
    path build_configs(const std::unordered_set<LocalPackage> &pkgs);

private:
    void prepareStep(Executor &e, Futures<void> &fs, std::atomic_bool &next_pass) const;
    void addTest(Test &cb, const String &name);

    template <class T>
    std::shared_ptr<PrepareConfigEntryPoint> build_configs1(const T &objs);

    //
public:
    Build(const SwContext &swctx, SwBuild &mb, const driver::cpp::Driver &driver);
    Build(const Build &);
    ~Build();

    void load_spec_file(const path &, const std::set<TargetSettings> &);
    void load_inline_spec(const path &);
    void load_dir(const path &);

    path build(const path &fn);
    void load_packages(const PackageIdSet &pkgs);
    void execute();
    void execute(CommandExecutionPlan &p) const;
    void prepare();
    bool prepareStep();
    Module loadModule(const path &fn) const;

private:
    std::vector<detail::EventCallback> events;

    // basic frontends
    void load_dll(const path &dll, const std::set<TargetSettings> &);
    void load_configless(const path &file_or_dir);

    // other frontends
    void cppan_load();
    void cppan_load(const path &fn);
    void cppan_load(const yaml &root, const String &root_name = {});
    bool cppan_check_config_root(const yaml &root);
};

}
