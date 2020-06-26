// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <primitives/hash.h>
#include <primitives/hash_combine.h>

namespace sw::support
{

SW_SUPPORT_API
String get_file_hash(const path &fn);

SW_SUPPORT_API
bool check_file_hash(const path &fn, const String &hash);

}
