// Copyright (C) 2018-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
