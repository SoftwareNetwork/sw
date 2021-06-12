// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

// 23: remove virtual method
// 24: add virtual method to core.target
// 25: core.target virtual methods update
// 26: C++20 transition
// 27: change OS::Version field to optional<>
// 28: Program::clone() result shared -> unique ptr
// 29: remove virtual method from core.target
#define SW_MODULE_ABI_VERSION 29
