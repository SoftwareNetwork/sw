/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2019 Egor Pugin
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

#include <primitives/sw/cl.h>
#include <sw/core/sw_context.h>

#define SUBCOMMAND_DECL(n) void cli_##n()
#define SUBCOMMAND_DECL2(n) void cli_##n(sw::SwContext &swctx)
#define SUBCOMMAND(n, d) SUBCOMMAND_DECL(n); SUBCOMMAND_DECL2(n);
#include "commands.inl"
#undef SUBCOMMAND

#define SUBCOMMAND(n, d) extern ::cl::SubCommand subcommand_##n;
#include "commands.inl"
#undef SUBCOMMAND

std::unique_ptr<sw::SwContext> createSwContext();
