// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "compiler/compiler.h"

#include <sw/builder/os.h>
#include <sw/core/target.h>

namespace sw
{

struct SW_DRIVER_CPP_API BuildSettings
{
    OS TargetOS;
    NativeToolchain Native;

    // other langs?
    // make polymorphic?

    BuildSettings() = default;
    BuildSettings(const TargetSettings &);

    String getTargetTriplet() const;
    TargetSettings getTargetSettings() const;
};

}
