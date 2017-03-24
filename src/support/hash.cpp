/*
 * Copyright (C) 2016-2017, Egor Pugin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "hash.h"

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
    // remove when server will be using strong_file_hash
    if (hash == sha256(fn))
        return true;
    if (hash == strong_file_hash(fn))
        return true;
    return false;
}
