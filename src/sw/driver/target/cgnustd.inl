// SPDX-License-Identifier: AGPL-3.0-or-later

#define STD(x) C_GNU_MACRO(x, c)
#include STD_INCLUDE
#undef STD

#define STD(x) C_GNU_MACRO(x, gnu)
#include STD_INCLUDE
#undef STD
