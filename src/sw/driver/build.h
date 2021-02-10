// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "checks_storage.h"
#include "command.h"
#include "target/base.h"

#include <sw/core/target.h>

namespace sw
{

struct SwBuild;
struct Build;
namespace driver::cpp { struct Driver; }
struct Module;
struct ModuleStorage;
struct SwContext;
struct ProgramDetector;
struct package_loader;
struct transform;

struct ModuleSwappableData
{
    using AddedTargets = std::vector<ITargetPtr>;

    const Package *known_target = nullptr;
    const PackageSettings *current_settings = nullptr;
    Resolver *resolver = nullptr;

    ModuleSwappableData();
    ModuleSwappableData(const ModuleSwappableData &) = delete;
    ModuleSwappableData &operator=(const ModuleSwappableData &) = delete;
    ModuleSwappableData(ModuleSwappableData &&) = default;
    ~ModuleSwappableData();

    void addTarget(ITargetPtr);
    void markAsDummy(const ITarget &);
    AddedTargets &getTargets();

    const PackageSettings &getSettings() const;

private:
    AddedTargets added_targets;
    AddedTargets dummy_children;
};

struct DriverData
{
    support::SourceDirMap source_dirs_by_source;
    std::unordered_map<PackageName, path> source_dirs_by_package;
    support::SourcePtr force_source;
};

struct SW_DRIVER_CPP_API Test : driver::CommandBuilder
{
    using driver::CommandBuilder::CommandBuilder;

    Test(const Test &) = default;
    Test &operator=(const Test &) = default;
    Test(const driver::CommandBuilder &b)
        : driver::CommandBuilder(b)
    {}

    void prepare(const Build &s)
    {
        // todo?
    }
};

struct SW_DRIVER_CPP_API Build : TargetBase
{
    ModuleSwappableData module_data;
    DriverData *dd = nullptr;
    std::unique_ptr<Checker> checker;
    SwBuild &main_build;
    transform &t;
    //const ProgramDetector &pd;

    //
    Build(transform &, SwBuild &);
    Build(const Build &) = delete;
    Build(Build &&) = default;
    //~Build();

    bool isKnownTarget(const PackageName &p) const;
    std::optional<path> getSourceDir(const Source &s, const PackageVersion &v) const;
    Resolver &getResolver() const;

    const PackageSettings &getExternalVariables() const;

    const SwContext &getContext() const;
    const LocalStorage &getLocalStorage() const;
    path getBuildDirectory() const;
    //FileStorage &getFileStorage() const;

    package_loader *load_package(const Package &);

    //const ProgramDetector &getProgramDetector() const { return pd; }

//private:
    //SwBuild &getMainBuild() const { return main_build; }
};

struct SW_DRIVER_CPP_API ExtendedBuild : Build
{
    using Base = Build;

    using Base::Base;

    const PackageSettings &getSettings() const { return module_data.getSettings(); }
    void addTarget(ITargetPtr t) { module_data.addTarget(std::move(t)); }
};

}
