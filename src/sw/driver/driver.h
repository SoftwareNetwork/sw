// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

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
    //std::vector<std::unique_ptr<Input>> getPredefinedInputs() const override;
    void setupBuild(SwBuild &) const override;

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
    TargetSettings getDllConfigSettings(SwBuild &swctx) const;

private:
    SwContext &swctx;
    mutable std::mutex m_bp;
    mutable std::optional<PackageIdSet> builtin_packages;
    using BuiltinInputs = std::unordered_map<Input*, PackageIdSet>;
    mutable BuiltinInputs builtin_inputs;
    PackageIdSet getBuiltinPackages(SwContext &) const;
    void getBuiltinInputs(SwContext &) const;

    mutable std::unique_ptr<SwBuild> b;
    std::unique_ptr<SwBuild> create_build(SwContext &swctx) const;
};

} // namespace driver::cpp

} // namespace sw
