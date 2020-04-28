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

#include <sw/core/driver.h>
#include <sw/core/input.h>
#include <sw/support/package_id.h>

#include <boost/bimap.hpp>
#include <boost/bimap/multiset_of.hpp>

namespace sw
{

struct Build;
struct Target;
struct NativeCompiledTarget;
struct SwBuild;
struct SwContext;
struct PrepareConfigEntryPoint;
struct TargetSettings;
struct PrepareConfigOutputData;
struct NativeBuiltinTargetEntryPoint;

path getDriverIncludeDir(Build &solution, Target &lib);
void addImportLibrary(const Build &b, NativeCompiledTarget &t);

namespace driver::cpp
{

enum class FrontendType;

struct SW_DRIVER_CPP_API Driver : IDriver
{
    Driver(SwContext &);
    Driver(const Driver &) = delete;
    Driver &operator=(const Driver &) = delete;
    virtual ~Driver();

    // this driver own api
    static void processConfigureAc(const path &p);

    // IDriver api
    void loadInputsBatch(const std::set<Input *> &) const override;
    std::vector<std::unique_ptr<Input>> detectInputs(const path &, InputType) const override;

    // frontends
    using AvailableFrontends = boost::bimap<boost::bimaps::multiset_of<FrontendType>, path>;
    static const AvailableFrontends &getAvailableFrontends();
    static const std::set<FrontendType> &getAvailableFrontendTypes();
    static const StringSet &getAvailableFrontendNames();
    static const FilesOrdered &getAvailableFrontendConfigFilenames();
    static bool isFrontendConfigFilename(const path &fn);
    static std::optional<FrontendType> selectFrontendByFilename(const path &fn);

    // service methods
    std::unordered_map<path, PrepareConfigOutputData> build_configs1(SwContext &, const std::set<Input *> &inputs) const;
    TargetSettings getDllConfigSettings(SwContext &swctx) const;

private:
    SwContext &swctx;
    mutable std::mutex m_bp;
    mutable std::optional<PackageIdSet> builtin_packages;
    std::unordered_map<Input*, PackageIdSet> builin_inputs;
    PackageIdSet getBuiltinPackages(SwContext &) const;

    mutable std::unique_ptr<SwBuild> b;
    std::unique_ptr<SwBuild> create_build(SwContext &swctx) const;
};

} // namespace driver::cpp

} // namespace sw
