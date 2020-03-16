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

#include "sw.h"

#define SOLUTION_VAR _solution

#define PRIVATE .Private +=
#define PUBLIC .Public +=
#define INTERFACE .Interface +=

#define add_library(t) auto &t = SOLUTION_VAR.addLibrary(#t)
#define add_executable(t) auto &t = SOLUTION_VAR.addExecutable(#t)

#define target_sources(t, ...) t __VA_ARGS__
#define target_include_directories(t, ...) t __VA_ARGS__
#define target_link_libraries(t, ...) t __VA_ARGS__
