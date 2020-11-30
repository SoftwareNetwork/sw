// SPDX-License-Identifier: GPL-3.0-or-later
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

struct ModuleSwappableData
{
    using AddedTargets = std::vector<ITargetPtr>;

    AllowedPackages known_targets;
    PackageSettings current_settings;

    ModuleSwappableData();
    ModuleSwappableData(const ModuleSwappableData &) = delete;
    ModuleSwappableData &operator=(const ModuleSwappableData &) = delete;
    ModuleSwappableData(ModuleSwappableData &&) = default;
    ~ModuleSwappableData();

    void addTarget(ITargetPtr);
    void markAsDummy(const ITarget &);
    AddedTargets &getTargets();

private:
    AddedTargets added_targets;
    AddedTargets dummy_children;
};

struct DriverData
{
    support::SourceDirMap source_dirs_by_source;
    std::unordered_map<PackageId, path> source_dirs_by_package;
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
    //const ProgramDetector &pd;

    //
    Build(SwBuild &);
    Build(const Build &) = delete;
    Build(Build &&) = default;
    //~Build();

    bool isKnownTarget(const PackageId &p) const;
    std::optional<path> getSourceDir(const Source &s, const Version &v) const;

    const PackageSettings &getExternalVariables() const;

    //const ProgramDetector &getProgramDetector() const { return pd; }
};

struct SW_DRIVER_CPP_API ExtendedBuild : Build
{
    using Base = Build;

    using Base::Base;

    const PackageSettings &getSettings() const { return module_data.current_settings; }
    void addTarget(ITargetPtr t) { module_data.addTarget(std::move(t)); }
};

}
