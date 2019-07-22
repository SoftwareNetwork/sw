// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <sw/core/driver.h>

namespace sw
{

struct Build;
struct ChecksStorage;
struct ModuleStorage;
struct SwBuild;

namespace driver::cpp
{

struct SW_DRIVER_CPP_API Driver : IDriver
{
    Driver();
    virtual ~Driver();

    // driver api
    PackageId getPackageId() const override;
    bool canLoad(const Input &) const override;
    void load(SwBuild &, const std::set<Input> &) override;
    String getSpecification() const override;
    //void execute();
    //bool prepareStep();

    // own
    ChecksStorage &getChecksStorage(const String &config) const;
    ChecksStorage &getChecksStorage(const String &config, const path &fn) const;
    ModuleStorage &getModuleStorage() const;

private:
    mutable std::unordered_map<String, std::unique_ptr<ChecksStorage>> checksStorages;
    std::unique_ptr<ModuleStorage> module_storage;
    //std::unique_ptr<Build> build;
};

} // namespace driver::cpp

std::optional<path> findConfig(const path &dir, const FilesOrdered &fe_s);

} // namespace sw
