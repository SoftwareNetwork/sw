// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

/// everything for building
SW_PACKAGE_API
void build(Solution &s);

/// everything for configuring
//SW_PACKAGE_API
//void configure(Build &s);

/// checker
SW_PACKAGE_API
void check(Checker &c);

// void setup() - current config?
// void fetch() - fetch sources
// void self(); // for self build instructions? why?
// void test(); // ?
// custom steps like in waf?
