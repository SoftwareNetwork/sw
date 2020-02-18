// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <sw/core/driver.h>
#include <sw/core/input.h>
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

enum class FrontendType
{
    Unspecified,

    // priority!
    Sw = 1,
    Cppan = 2,
    Cargo = 3, // rust
    Dub = 4, // d
    Composer = 5, // php
};

struct SW_DRIVER_CPP_API Driver : IDriver
{
    Driver();
    Driver(const Driver &) = delete;
    Driver &operator=(const Driver &) = delete;
    virtual ~Driver();

    // this driver own api
    void processConfigureAc(const path &p);

    // IDriver api
    void loadInputsBatch(SwContext &, const std::set<Input *> &) const override;
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
    mutable std::mutex m_bp;
    mutable std::optional<PackageIdSet> builtin_packages;
    PackageIdSet getBuiltinPackages(SwContext &) const;

    mutable std::unique_ptr<SwBuild> b;
    std::unique_ptr<SwBuild> create_build(SwContext &swctx) const;
};

struct DriverInput
{
    FrontendType fe_type = FrontendType::Unspecified;
};

struct SpecFileInput : Input, DriverInput
{
    using Input::Input;

    // at the moment only sw is batch loadable
    bool isBatchLoadable() const override;

    // everything else is parallel loadable
    bool isParallelLoadable() const override;

    std::unique_ptr<Specification> getSpecification() const override;
    EntryPointsVector load1(SwContext &swctx) override;
    void setEntryPoints(const EntryPointsVector &in) override;
};

} // namespace driver::cpp

String gn2suffix(PackageVersionGroupNumber gn);

} // namespace sw
