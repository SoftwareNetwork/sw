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

#include <any>
#include <variant>

namespace sw
{

struct Build;
namespace driver::cpp { struct Driver; }
struct Module;
struct ModuleStorage;
struct SwContext;
struct ModuleSwappableData;
struct PrepareConfigEntryPoint;

template <class T>
struct ExecutionPlan;

using FilesMap = std::unordered_map<path, path>;

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

struct SimpleBuild : TargetBase
{
};

struct SW_DRIVER_CPP_API Build : SimpleBuild
{
    using Base = SimpleBuild;

    using CommandExecutionPlan = ExecutionPlan<builder::Command>;

    // most important
    SwBuild &main_build;
private:
    TargetSettings host_settings;
public:

    //
    std::vector<TargetBaseTypePtr> dummy_children;
    const ModuleSwappableData *module_data = nullptr;
    SourceDirMap source_dirs_by_source;
    int command_storage = 0;
    Checker checker;

    SwContext &getContext() const;
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
    const ModuleSwappableData &getModuleData() const;
    PackageVersionGroupNumber getCurrentGroupNumber() const;

    // tests
    // TODO: implement some of https://cmake.org/cmake/help/latest/manual/cmake-properties.7.html#properties-on-tests
    Commands tests;
    Test addTest(const ExecutableTarget &t);
    Test addTest(const String &name, const ExecutableTarget &t);
    Test addTest();
    Test addTest(const String &name);
    path getTestDir() const;

    template <class T>
    std::shared_ptr<PrepareConfigEntryPoint> build_configs1(const T &objs);

private:
    void addTest(Test &cb, const String &name);

    //
public:
    Build(SwBuild &);
    Build(const Build &);
    ~Build();

    void load_inline_spec(const path &);
    void load_dir(const path &);

    void load_packages(const PackageIdSet &pkgs);
    Module loadModule(const path &fn) const;

private:
    // basic frontends
    void load_configless(const path &file_or_dir);

    // other frontends
    void cppan_load();
    void cppan_load(const path &fn);
    void cppan_load(const yaml &root, const String &root_name = {});
    bool cppan_check_config_root(const yaml &root);
};

String gn2suffix(PackageVersionGroupNumber gn);

}
