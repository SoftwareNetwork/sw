// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "sw_abi_version.h"

SW_PACKAGE_API
int sw_get_module_abi_version()
{
    return SW_MODULE_ABI_VERSION;
}
