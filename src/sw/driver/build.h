// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

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

struct ModuleSwappableData
{
    PackageIdSet known_targets;
    TargetSettings current_settings;
    std::vector<ITargetPtr> added_targets;
};

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

struct SW_DRIVER_CPP_API SimpleBuild : TargetBase
{
    // public functions for sw frontend
};

struct SW_DRIVER_CPP_API Build : SimpleBuild
{
    using Base = SimpleBuild;

    ModuleSwappableData module_data;
    SourceDirMap source_dirs_by_source;
    std::unordered_map<PackageId, path> source_dirs_by_package;
    Checker checker;

    //
    bool isKnownTarget(const LocalPackage &p) const;
    path getSourceDir(const LocalPackage &p) const;
    std::optional<path> getSourceDir(const Source &s, const Version &v) const;

    const TargetSettings &getExternalVariables() const;

    //
public:
    Build(SwBuild &);

    //Module loadModule(const path &fn) const;

    // move to some other place?
    std::vector<NativeCompiledTarget *> cppan_load(yaml &root, const String &root_name = {});

private:
    std::vector<NativeCompiledTarget *> cppan_load1(const yaml &root, const String &root_name);
};

}
