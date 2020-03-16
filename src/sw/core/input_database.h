/*
 * SW - Build System and Package Manager
 * Copyright (C) 2020 Egor Pugin
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

#include "input.h"

#include <sw/manager/database.h>

namespace sw
{

struct InputDatabase : Database
{
    InputDatabase(const path &p);

    void setupInput(Input &) const;

    // helper
    size_t addInputFile(const path &file) const;
};

} // namespace sw
