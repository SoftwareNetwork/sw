// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <sw/core/driver.h>
#include <sw/manager/package_id.h>

#include <boost/bimap.hpp>
#include <boost/bimap/multiset_of.hpp>

namespace sw
{

struct Build;
struct SwBuild;
struct SwContext;
struct PrepareConfigEntryPoint;
struct TargetSettings;

namespace driver::cpp
{

enum class FrontendType;

struct SW_DRIVER_CPP_API Driver : IDriver
{
    Driver();
    Driver(const Driver &) = delete;
    Driver &operator=(const Driver &) = delete;
    virtual ~Driver();

    // this driver own api
    void processConfigureAc(const path &p);

    // IDriver api
    //FilesOrdered canLoadInput(const RawInput &) const override;
    //EntryPointsVector createEntryPoints(SwContext &, const std::vector<RawInput> &) const override;
    //std::unique_ptr<Specification> getSpecification(const RawInput &) const override;
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
    template <class T>
    std::shared_ptr<PrepareConfigEntryPoint> build_configs1(SwContext &, const T &objs) const;

private:
    // load things
    std::unordered_map<PackageId, EntryPointsVector1> load_packages(SwContext &, const PackageIdSet &pkgs) const;

    //
    mutable std::mutex m_bp;
    mutable std::optional<PackageIdSet> builtin_packages;
    PackageIdSet getBuiltinPackages(SwContext &) const;
};

//FilesOrdered findConfig(const path &dir, const FilesOrdered &fe_s);
//String toString(FrontendType T);

} // namespace driver::cpp

String gn2suffix(PackageVersionGroupNumber gn);

} // namespace sw
