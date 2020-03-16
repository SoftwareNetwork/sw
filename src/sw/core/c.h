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

#ifndef _SW_CORE_C_H_
#define _SW_CORE_C_H_

#include <stddef.h>

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

typedef struct
{
} sw_driver_input_t;

typedef struct
{
    /* callee must keep result string in memory */
    const char *(*get_package_id)(void);

    /**/
    int (*can_load)(sw_driver_input_t *);

    /* end is indicated with 0 */
    void (*load)(sw_driver_input_t **);

} sw_driver_t;

#endif
