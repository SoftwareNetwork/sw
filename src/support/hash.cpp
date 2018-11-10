// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "hash.h"

#include <primitives/symbol.h>

#define CPPAN_CONFIG_HASH_SHORT_LENGTH 12

String shorten_hash(const String &data)
{
    return shorten_hash(data, CPPAN_CONFIG_HASH_SHORT_LENGTH);
}

String sha256_short(const String &data)
{
    return shorten_hash(sha256(data));
}

String hash_config(const String &c)
{
    return sha256_short(c);
}

bool check_file_hash(const path &fn, const String &hash)
{
    return hash == strong_file_hash(fn);
}

static path getCurrentModuleName()
{
    return primitives::getModuleNameForSymbol(primitives::getCurrentModuleSymbol());
}

String getCurrentModuleNameHash()
{
    return shorten_hash(blake2b_512(getCurrentModuleName().u8string()));
}
