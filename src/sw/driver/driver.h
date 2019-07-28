// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <sw/core/driver.h>

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
    // priority!
    Sw = 1,
    Cppan = 2,
};

struct SW_DRIVER_CPP_API Driver : IDriver
{
    Driver();
    Driver(const Driver &) = delete;
    Driver &operator=(const Driver &) = delete;
    virtual ~Driver();

    // driver api
    PackageId getPackageId() const override;
    bool canLoad(const RawInput &) const override;
    EntryPontsVector load(SwContext &, const std::vector<RawInput> &) const override;
    String getSpecification(const RawInput &) const override;

    // frontends
    using AvailableFrontends = boost::bimap<boost::bimaps::multiset_of<FrontendType>, path>;
    static const AvailableFrontends &getAvailableFrontends();
    static const std::set<FrontendType> &getAvailableFrontendTypes();
    static const StringSet &getAvailableFrontendNames();
    static const FilesOrdered &getAvailableFrontendConfigFilenames();
    static bool isFrontendConfigFilename(const path &fn);
    static std::optional<FrontendType> selectFrontendByFilename(const path &fn);

private:
    // load things
    EntryPontsVector1 load_spec_file(SwContext &, const path &) const;
    std::unordered_map<PackageId, EntryPontsVector1> load_packages(SwContext &, const PackageIdSet &pkgs) const;

    template <class T>
    std::shared_ptr<PrepareConfigEntryPoint> build_configs1(SwContext &, const T &objs) const;
};

std::optional<path> findConfig(const path &dir, const FilesOrdered &fe_s);
String toString(FrontendType T);

} // namespace driver::cpp

} // namespace sw
