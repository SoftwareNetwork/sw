// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <sw/support/package.h>

namespace sw
{

struct NativeBuiltinTargetEntryPoint;

// input_hash, EP
using BuiltinEntryPoints = std::vector<std::tuple<size_t, std::unique_ptr<NativeBuiltinTargetEntryPoint>>>;

BuiltinEntryPoints load_builtin_entry_points();
PackageIdSet load_builtin_packages();

} // namespace sw
