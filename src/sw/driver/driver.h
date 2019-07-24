// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "checks_storage.h"

#include <sw/core/driver.h>

#include <boost/bimap.hpp>
#include <boost/bimap/multiset_of.hpp>

namespace sw
{

struct Build;
struct SwBuild;
struct SwContext;

namespace driver::cpp
{

enum class FrontendType
{
    // priority!
    Sw = 1,
    Cppan = 2,
};

SW_DRIVER_CPP_API
String toString(FrontendType T);

struct SW_DRIVER_CPP_API Driver : IDriver
{
    Driver();
    Driver(const Driver &) = delete;
    Driver &operator=(const Driver &) = delete;
    virtual ~Driver();

    // driver api
    PackageId getPackageId() const override;
    bool canLoad(const Input &) const override;
    void load(SwBuild &, const std::set<Input> &) override;
    String getSpecification() const override;

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
    void load_spec_file(SwBuild &, const path &, const std::set<TargetSettings> &);
    path build_cpp_spec(SwContext &, const path &fn);
    void load_dll(SwBuild &swctx, const path &dll, const std::set<TargetSettings> &);
};

std::optional<path> findConfig(const path &dir, const FilesOrdered &fe_s);

} // namespace driver::cpp

} // namespace sw
