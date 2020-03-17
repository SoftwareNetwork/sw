/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2020 Egor Pugin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

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

struct DriverData
{
    SourceDirMap source_dirs_by_source;
    std::unordered_map<PackageId, path> source_dirs_by_package;
    SourcePtr force_source;
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
    DriverData *dd = nullptr;
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
