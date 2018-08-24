// Copyright (C) 2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <sw/builder/driver.h>

namespace sw::driver::cppan
{

struct SW_DRIVER_CPPAN_API CppanDriver : ::sw::Driver
{
    static inline const path filename = "cppan.yml";

    virtual ~CppanDriver() = default;

    path getConfigFilename() const override;

    PackageScriptPtr load(const path &file_or_dir) const override;
    bool execute(const path &file_or_dir) const override;

    // not implemented yet
    void fetch(const path &file_or_dir) const override {}
    PackageScriptPtr fetch_and_load(const path &file_or_dir) const override { return {}; }
    PackageScriptPtr build(const path &file_or_dir) const override { return {}; }
    String getName() const override { return "cppan"; }
    bool buildPackage(const PackageId &pkg) const override { return false; }
};

} // namespace sw::driver
