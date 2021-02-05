// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "sw_abi_version.h"

#ifndef SW_PACKAGE_API
#define SW_PACKAGE_API
#define INLINE inline
#endif

#ifndef INLINE
#define INLINE
#endif

SW_PACKAGE_API
INLINE int sw_get_module_abi_version()
{
    return SW_MODULE_ABI_VERSION;
}

#undef INLINE
