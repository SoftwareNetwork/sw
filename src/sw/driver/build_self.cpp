// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include "builtin_input.h"
#include "entry_point.h"

#include <sw/core/sw_context.h>
#include <sw/core/target.h>

#include <primitives/executor.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "build.self");

#define SW_PACKAGE_API
#include "sw.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4005) // warning C4005: 'XXX': macro redefinition
#endif

#include <build_self.generated.h>
