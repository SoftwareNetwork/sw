// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "c.h"
#include "c.hpp"

const char *sw_driver_get_package_id(void)
{
    return "org.sw.driver.c-0.3.1";
}

int sw_driver_can_load(sw_driver_input_t *)
{
    return 0;
}

void sw_driver_load(sw_driver_input_t **)
{
    SW_UNIMPLEMENTED;
}

SW_CORE_API
sw_driver_t sw_create_driver(void)
{
    sw_driver_t d;
    d.get_package_id = sw_driver_get_package_id;
    d.can_load = sw_driver_can_load;
    d.load = sw_driver_load;
    return d;
}

namespace sw
{

CDriver::CDriver(create_driver cd)
{
    d = cd();
}

CDriver::~CDriver() {}

// driver api
PackageId CDriver::getPackageId() const
{
    return d.get_package_id();
}

bool CDriver::canLoad(const Input &) const
{
    return d.can_load(0);
}

void CDriver::load(const std::set<Input> &)
{
    d.load(0);
}

}
