// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <sw/builder/driver.h>

namespace sw::driver::cpp
{

struct SW_DRIVER_CPP_API Driver : ::sw::Driver
{
    virtual ~Driver() = default;

    path getConfigFilename() const override;

    void fetch(const path &file_or_dir) const override;
    PackageScriptPtr fetch_and_load(const path &file_or_dir) const override;
    PackageScriptPtr load(const path &file_or_dir) const override;
    bool execute(const path &file_or_dir) const override;
    String getName() const override { return "cpp"; }
};

} // namespace sw::driver
