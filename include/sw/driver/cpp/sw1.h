#pragma once

/// everything for building
SW_PACKAGE_API
void build(sw::Solution &s);

/// everything for configuring
SW_PACKAGE_API
void configure(sw::Build &s);

/// checker
SW_PACKAGE_API
void check(sw::Checker &c);

// void setup() - current config?
// void fetch() - fetch sources
// void self(); // for self build instructions? why?
// void test(); // ?
