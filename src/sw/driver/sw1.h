// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

/// everything for building
SW_PACKAGE_API
void build(Solution &s);

/// everything for configuring
SW_PACKAGE_API
void configure(Build &s);

/// checker
SW_PACKAGE_API
void check(Checker &c);

// void setup() - current config?
// void fetch() - fetch sources
// void self(); // for self build instructions? why?
// void test(); // ?
// custom steps like in waf?
