// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef _SW_CORE_C_H_
#define _SW_CORE_C_H_

typedef struct
{
} sw_driver_input_t;

typedef struct
{
    // callee must keep result string in memory
    const char *(*get_package_id)(void);

    //
    int (*can_load)(sw_driver_input_t *);

    // end is indicated with 0
    void (*load)(sw_driver_input_t **);

} sw_driver_t;

#endif
