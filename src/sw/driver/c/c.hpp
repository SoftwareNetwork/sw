// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "c.h"
#include "../driver.h"

namespace sw
{

/*struct SW_DRIVER_CPP_API CDriver : IDriver
{
    using create_driver = sw_driver_t(*)(void);

    CDriver(create_driver cd);
    virtual ~CDriver();

    PackageId getPackageId() const override;
    bool canLoad(RawInputData &) const override;
    EntryPointsVector createEntryPoints(SwContext &, const std::vector<RawInput> &) const override;

private:
    sw_driver_t d;
};*/

}
