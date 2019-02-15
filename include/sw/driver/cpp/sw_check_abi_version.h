#pragma once

#include "sw_abi_version.h"

SW_PACKAGE_API
int sw_get_module_abi_version()
{
    return SW_MODULE_ABI_VERSION;
}
