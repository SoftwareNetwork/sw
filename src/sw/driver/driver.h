// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <sw/core/sw_context.h>

namespace sw
{

struct Build;
struct ChecksStorage;
struct ModuleStorage;

namespace driver::cpp
{

struct SW_DRIVER_CPP_API Driver : IDriver
{
    Driver(const SwContext &swctx);
    virtual ~Driver();

    // driver api
    PackageId getPackageId() const override;
    bool canLoad(const Input &) const override;
    void load(const std::set<Input> &) override;

    // own
    ChecksStorage &getChecksStorage(const String &config) const;
    ChecksStorage &getChecksStorage(const String &config, const path &fn) const;
    ModuleStorage &getModuleStorage() const;

private:
    const SwContext &swctx;
    mutable std::unordered_map<String, std::unique_ptr<ChecksStorage>> checksStorages;
    std::unique_ptr<ModuleStorage> module_storage;
    std::unique_ptr<Build> build;
};

} // namespace driver::cpp

} // namespace sw
