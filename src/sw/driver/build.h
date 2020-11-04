// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "checks_storage.h"
#include "command.h"
#include "target/base.h"

#include <sw/core/build.h>
#include <sw/core/target.h>

namespace sw
{

struct Build;
namespace driver::cpp { struct Driver; }
struct Module;
struct ModuleStorage;
struct SwContext;
struct ProgramDetector;

struct ModuleSwappableData
{
    AllowedPackages known_targets;
    TargetSettings current_settings;
    std::vector<ITargetPtr> added_targets;
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
    std::shared_ptr<Checker> checker;
    //const ProgramDetector &pd;

    //
    Build(SwBuild &/*, const ProgramDetector &*/);

    bool isKnownTarget(const LocalPackage &p) const;
    path getSourceDir(const LocalPackage &p) const;
    std::optional<path> getSourceDir(const Source &s, const Version &v) const;

    const TargetSettings &getExternalVariables() const;

    //const ProgramDetector &getProgramDetector() const { return pd; }
};

struct SW_DRIVER_CPP_API ExtendedBuild : Build
{
    using Base = Build;

    using Base::Base;

    const TargetSettings &getSettings() const { return module_data.current_settings; }
    void addTarget(const ITargetPtr &t) { module_data.added_targets.push_back(t); }
};

}
