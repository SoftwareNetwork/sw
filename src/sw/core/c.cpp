/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2020 Egor Pugin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "c.h"
#include "c.hpp"

#include <sw/manager/package_id.h>

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

/*CDriver::CDriver(create_driver cd)
{
    d = cd();
}

CDriver::~CDriver() {}

// driver api
PackageId CDriver::getPackageId() const
{
    return String(d.get_package_id());
}

bool CDriver::canLoad(RawInputData &) const
{
    return d.can_load(0);
}

CDriver::EntryPointsVector CDriver::createEntryPoints(SwContext &, const std::vector<RawInput> &) const
{
    SW_UNIMPLEMENTED;
    d.load(0);
}*/

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

#ifndef __APPLE__
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
#endif

sw_target_t *sw_add_executable(sw_build_t *, const char *name)
{
    return 0;
}

void sw_set_target_property(sw_target_t *, const char *property, const char *value)
{

}

void sw_add_target_source(sw_target_t *, const char *filename)
{

}
