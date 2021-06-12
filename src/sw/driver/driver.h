// SPDX-License-Identifier: AGPL-3.0-only
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
struct my_package_loader;

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

    //PackageName get_package_name();
    const PackageId &get_package() const override;
    const PackageSettings &get_properties() const override;

    // IDriver api
    //void loadInputsBatch(const std::set<Input *> &) const override;
    std::vector<std::unique_ptr<Input>> detectInputs(const path &) const;
    std::vector<std::unique_ptr<Input>> detectInputs(const path &, InputType) const;
    std::unique_ptr<Input> getInput(const Package &) const;
    //std::vector<std::unique_ptr<Input>> getPredefinedInputs() const override;
    //void setupBuild(SwBuild &) const override;

    package_loader *load_package(const Package &) override;
    std::vector<package_loader *> load_packages(const path &) override;

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

    CommandStorage *getCommandStorage(const Target &t) const;

private:
    PackageId id;
    transform &transform_;
    SwContext &swctx;
    std::unique_ptr<struct BuiltinStorage> bs;
    std::unique_ptr<struct ConfigStorage> cs;
    std::vector<std::unique_ptr<my_package_loader>> loaders;
    std::unordered_map<PackageId, std::unique_ptr<my_package_loader>> loaders1;
    std::unordered_map<PackageId, std::unique_ptr<my_package_loader>> loaders2;

    std::unique_ptr<SwBuild> create_build(SwContext &swctx) const;
    std::vector<package_loader *> load_packages(std::vector<std::unique_ptr<Input>> &&);
};

} // namespace driver::cpp

} // namespace sw
