// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <primitives/yaml.h>

namespace sw
{

struct Build;
struct NativeCompiledTarget;

namespace driver::cpp::frontend::cppan
{

std::vector<NativeCompiledTarget *> cppan_load(Build &, yaml &root, const String &root_name = {});

} // namespace driver::cpp::frontend::cppan

} // namespace sw
