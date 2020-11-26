// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <sw/support/package.h>

namespace sw
{

struct NativeBuiltinTargetEntryPoint;

// input hash, EP, pkgs
using BuiltinInputs = std::vector<std::tuple<size_t, std::unique_ptr<NativeBuiltinTargetEntryPoint>, PackageIdSet>>;

BuiltinInputs load_builtin_inputs();

} // namespace sw
