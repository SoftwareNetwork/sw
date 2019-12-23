// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "hash.h"

String get_file_hash(const path &fn)
{
    return strong_file_hash(fn);
}

bool check_file_hash(const path &fn, const String &hash)
{
    return hash == get_file_hash(fn);
}

size_t get_specification_hash(const String &input)
{
    return boost::hash<String>()(input);
}

size_t add_specification_hash(size_t &hash, const String &input)
{
    return hash_combine(hash, get_specification_hash(input));
}
