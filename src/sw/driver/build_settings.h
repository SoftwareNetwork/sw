// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

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

    String getConfig() const;
    String getTargetTriplet() const;

    TargetSettings getTargetSettings() const;

    bool operator<(const BuildSettings &rhs) const;
    bool operator==(const BuildSettings &rhs) const;
};

}
