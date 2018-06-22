// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

// builder stuff
#include <solution.h>
#include <suffix.h>

// manager stuff
#include <resolver.h>

// support stuff
#include <primitives/hash.h>
#include <primitives/http.h>
#include <boost/algorithm/string.hpp>

/// everything for building
SW_PACKAGE_API
void build(sw::Solution &s);

/// checker
SW_PACKAGE_API
void check(sw::Checker &c);

/// everything for configuring
SW_PACKAGE_API
void configure(sw::Solution &s);

// void setup() - current config?
// void fetch() - fetch sources
// void self(); // for self build instructions? why?
// void test(); // ?

//namespace sw {}
using namespace sw;
using namespace sw::driver::cpp;

// user code will use
// using namespace sw::vN; // where N - version of sw api
