// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

// builder stuff
#include <solution.h>
#include <suffix.h>
#include <jumppad.h>
#include <compiler_helpers.h>
#include <module.h>
#include <target/all.h>

// manager stuff
#include <resolver.h>

// support stuff
//#include <primitives/hash.h>
//#include <primitives/http.h>
#include <boost/algorithm/string.hpp>

//namespace sw {}
using namespace sw;
using namespace sw::driver::cpp;

// user code will use
// using namespace sw::vN; // where N - version of sw api

// disable custom pragma warnings
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4068)
#endif

#ifdef _MSC_VER
#include "sw1.h"
#endif
