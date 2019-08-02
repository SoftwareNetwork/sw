// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "c.h"
#include "driver.h"

namespace sw
{

struct SW_CORE_API CDriver : IDriver
{
    using create_driver = sw_driver_t(*)(void);

    CDriver(create_driver cd);
    virtual ~CDriver();

    PackageId getPackageId() const override;
    bool canLoad(const RawInput &) const override;
    EntryPointsVector createEntryPoints(SwContext &, const std::vector<RawInput> &) const override;

private:
    sw_driver_t d;
};

}
