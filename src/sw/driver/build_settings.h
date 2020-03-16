/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2020 Egor Pugin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

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

    //bool operator<(const BuildSettings &rhs) const;
    //bool operator==(const BuildSettings &rhs) const;
};

}
