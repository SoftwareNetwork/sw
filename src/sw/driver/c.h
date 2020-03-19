/*
 * SW - Build System and Package Manager
 * Copyright (C) 2019-2020 Egor Pugin
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

#ifndef _SW_C_H_
#define _SW_C_H_

#ifdef __cplusplus
#include "sw.h"
#endif

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* memory allocations */

SW_CORE_API
void *sw_malloc(size_t);

SW_CORE_API
void sw_free(void *);

SW_CORE_API
void *sw_realloc(void *, size_t);

SW_CORE_API
void *sw_calloc(size_t, size_t);

//SW_CORE_API
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

SW_CORE_API sw_executable_target_t *sw_add_executable(sw_build_t *, const char *name);
SW_CORE_API sw_library_target_t *sw_add_library(sw_build_t *, const char *name);
SW_CORE_API sw_static_library_t *sw_add_static_library(sw_build_t *, const char *name);
SW_CORE_API sw_shared_library_t *sw_add_shared_library(sw_build_t *, const char *name);

SW_CORE_API void sw_set_target_property(sw_target_t *, const char *property, const char *value);

SW_CORE_API void sw_add_target_source(sw_target_t *, const char *filename);
SW_CORE_API void sw_add_target_regex(sw_target_t *, const char *regex);
SW_CORE_API void sw_add_target_recursive_regex(sw_target_t *, const char *regex);

#ifdef __cplusplus
}
#endif

#endif
