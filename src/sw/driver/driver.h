// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <sw/core/sw_context.h>

namespace sw::driver::cpp
{

struct SW_DRIVER_CPP_API Driver : IDriver
{
    Driver(const SwContext &swctx);
    virtual ~Driver() = default;

    PackageId getPackageId() const override;
    bool canLoad(const Input &) const override;
    void load(const Input &) override;
};

}
