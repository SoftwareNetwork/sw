// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>

#include "hash.h"

namespace sw::support
{

String get_file_hash(const path &fn)
{
    return strong_file_hash_file(fn);
}

bool check_file_hash(const path &fn, const String &hash)
{
    return hash == get_file_hash(fn);
}

}
