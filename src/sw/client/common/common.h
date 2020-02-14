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

#include <sw/core/sw_context.h>
#include <sw/core/target.h>

struct Options;

std::unique_ptr<sw::SwContext> createSwContext(const Options &);
void setHttpSettings(const Options &);

struct Program
{
    String name;
    String desc;

    struct data
    {
        sw::TargetContainer *c = nullptr;
    };

    using Container = sw::PackageVersionMapBase<data, std::unordered_map, primitives::version::VersionMap>;

    Container releases;
    Container prereleases;
};

using Programs = std::vector<Program>;

Programs list_compilers(sw::SwContext &);

String list_predefined_targets(sw::SwContext &);
String list_programs(sw::SwContext &);
