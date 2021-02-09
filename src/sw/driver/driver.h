// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <sw/core/driver.h>
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
struct PackageSettings;
struct PrepareConfigOutputData;
struct NativeBuiltinTargetEntryPoint;
struct transform;

namespace driver::cpp
{

enum class FrontendType;

struct SW_DRIVER_CPP_API Driver : IDriver
{
    Driver(transform &, SwContext &);
    Driver(const Driver &) = delete;
    Driver &operator=(const Driver &) = delete;
    virtual ~Driver();

    // this driver own api
    static void processConfigureAc(const path &p);

    // make IDriver api?
    static PackageName getPackageId();

    // IDriver api
    void loadInputsBatch(const std::set<Input *> &) const override;
    std::vector<std::unique_ptr<Input>> detectInputs(const path &) const;
    std::vector<std::unique_ptr<Input>> detectInputs(const path &, InputType) const override;
    std::unique_ptr<Input> getInput(const Package &) const override;
    //std::vector<std::unique_ptr<Input>> getPredefinedInputs() const override;
    void setupBuild(SwBuild &) const override;

    package_loader_ptr load_package(const Package &) override;
    std::vector<package_loader_ptr> load_packages(const path &) override;

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
    PackageSettings getDllConfigSettings(/*SwBuild &swctx*/) const;

    SwContext &getContext() const { return swctx; }
    transform &get_transform() const { return transform_; }

private:
    transform &transform_;
    SwContext &swctx;
    std::unique_ptr<struct BuiltinStorage> bs;
    std::unique_ptr<struct ConfigStorage> cs;

    std::unique_ptr<SwBuild> create_build(SwContext &swctx) const;
    std::vector<package_loader_ptr> load_packages(std::vector<std::unique_ptr<Input>> &&);
};

} // namespace driver::cpp

} // namespace sw
