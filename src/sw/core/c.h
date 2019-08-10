/*
 * Copyright (C) 2019 Egor Pugin <egor.pugin@gmail.com>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
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
