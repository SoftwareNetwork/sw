// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "entry_point.h"

namespace sw
{

struct NativeBuiltinTargetEntryPoint;

struct ConfigDependency
{
    EntryPointFunctions bfs;
    std::unordered_map<UnresolvedPackageName, PackageName> resolver_cache;

    void add_pair(const String &u, const String &n) { resolver_cache.emplace(u, n); }
};

using BuiltinEntryPoints = std::vector<ConfigDependency>;

BuiltinEntryPoints load_builtin_entry_points();

} // namespace sw
