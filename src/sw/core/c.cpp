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
    return String(d.get_package_id());
}

bool CDriver::canLoad(const RawInput &) const
{
    return d.can_load(0);
}

CDriver::EntryPointsVector CDriver::createEntryPoints(SwContext &, const std::vector<RawInput> &) const
{
    SW_UNIMPLEMENTED;
    d.load(0);
}

}

// memory

void *sw_malloc(size_t size)
{
    return malloc(size);
}

void sw_free(void *ptr)
{
    free(ptr);
}

void *sw_realloc(void *ptr, size_t size)
{
    return realloc(ptr, size);
}

void *sw_calloc(size_t num, size_t size)
{
    return calloc(num, size);
}

void *sw_aligned_alloc(size_t alignment, size_t size)
{
#ifdef _MSC_VER
    // must be freed with _aligned_free
    return _aligned_malloc(alignment, size);
#else
    // must be freed with free/realloc
    return aligned_alloc(alignment, size);
#endif
}
