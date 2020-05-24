// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

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
