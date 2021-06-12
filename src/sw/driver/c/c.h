// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2019-2020 Egor Pugin <egor.pugin@gmail.com>

#ifndef _SW_C_H_
#define _SW_C_H_

#ifdef __cplusplus
#include "../sw.h"
#endif

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* memory allocations */

SW_DRIVER_CPP_API
void *sw_malloc(size_t);

SW_DRIVER_CPP_API
void sw_free(void *);

SW_DRIVER_CPP_API
void *sw_realloc(void *, size_t);

SW_DRIVER_CPP_API
void *sw_calloc(size_t, size_t);

//SW_DRIVER_CPP_API
//void *sw_aligned_alloc(size_t, size_t);

/* ...  */

typedef struct sw_driver_input_t sw_driver_input_t;

typedef struct
{
    /* callee must keep result string in memory */
    const char *(*get_package_id)(void);

    /**/
    int (*can_load)(sw_driver_input_t *);

    /* end is indicated with 0 */
    void (*load)(sw_driver_input_t **);

} sw_driver_t;

#ifdef __cplusplus
#define TYPE(f, t) typedef f sw_##t##_t;
#include "c.types.inl"
#undef TYPE
#else
#define TYPE(f, t) typedef void sw_##t##_t;
#include "c.types.inl"
#undef TYPE
#endif

SW_DRIVER_CPP_API sw_executable_target_t *sw_add_executable(sw_build_t *, const char *name);
SW_DRIVER_CPP_API sw_library_target_t *sw_add_library(sw_build_t *, const char *name);
SW_DRIVER_CPP_API sw_static_library_target_t *sw_add_static_library(sw_build_t *, const char *name);
SW_DRIVER_CPP_API sw_shared_library_target_t *sw_add_shared_library(sw_build_t *, const char *name);

SW_DRIVER_CPP_API void sw_set_target_property(sw_target_t *, const char *property, const char *value);

SW_DRIVER_CPP_API void sw_add_target_source(sw_target_t *, const char *filename);
SW_DRIVER_CPP_API void sw_add_target_regex(sw_target_t *, const char *regex);
SW_DRIVER_CPP_API void sw_add_target_recursive_regex(sw_target_t *, const char *regex);

#ifdef __cplusplus
}
#endif

#endif
