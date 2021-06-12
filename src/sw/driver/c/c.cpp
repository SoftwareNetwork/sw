// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

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

/*SW_DRIVER_CPP_API
sw_driver_t sw_create_driver(void)
{
    sw_driver_t d;
    d.get_package_id = sw_driver_get_package_id;
    d.can_load = sw_driver_can_load;
    d.load = sw_driver_load;
    return d;
}*/

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

template <class T>
static T &toTarget(sw_target_t *in)
{
    auto &t = (Target &)*in;
    return dynamic_cast<T &>(t);
    //t.getType();
    //return t;
}

sw_executable_target_t *sw_add_executable(Build *b, const char *name)
{
    auto &t = b->add<Executable>(name);
    return &t;
}

sw_library_target_t *sw_add_library(Build *b, const char *name)
{
    auto &t = b->add<Library>(name);
    return &t;
}

sw_static_library_target_t *sw_add_static_library(Build *b, const char *name)
{
    auto &t = b->add<StaticLibrary>(name);
    return &t;
}

sw_shared_library_target_t *sw_add_shared_library(Build *b, const char *name)
{
    auto &t = b->add<SharedLibrary>(name);
    return &t;
}

void sw_set_target_property(Target *t, const char *property, const char *value)
{
    if (strcmp(property, "API_NAME") == 0)
        toTarget<NativeCompiledTarget>(t).ApiNames.insert(value);
}

void sw_add_target_source(Target *t, const char *filename)
{
    toTarget<NativeCompiledTarget>(t) += filename;
}

void sw_add_target_regex(Target *t, const char *filename)
{
    toTarget<NativeCompiledTarget>(t) += FileRegex({}, filename, false);
}

void sw_add_target_recursive_regex(Target *t, const char *filename)
{
    toTarget<NativeCompiledTarget>(t) += FileRegex({}, filename, true);
}
