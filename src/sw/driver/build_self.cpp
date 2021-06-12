// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include "builtin_input.h"
#include "entry_point.h"

#define SW_PACKAGE_API
#include "sw.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4005) // warning C4005: 'XXX': macro redefinition
#endif

#include <build_self.generated.h>
