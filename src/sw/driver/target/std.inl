// SPDX-License-Identifier: AGPL-3.0-only

#define STD_INCLUDE "cstd.inl"
#define C_GNU_MACRO(x, l) STD_MACRO(x, l)
#include "cgnustd.inl"
#undef C_GNU_MACRO
#undef STD_INCLUDE

#define STD_INCLUDE "cppstd.inl"
#define C_GNU_MACRO(x, l) STD_MACRO(x, l ## pp)
#include "cgnustd.inl"
#undef C_GNU_MACRO
#undef STD_INCLUDE
