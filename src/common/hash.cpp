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

#include <random>

#include <boost/algorithm/string.hpp>
#include <openssl/evp.h>

 // keep always digits,lowercase,uppercase
static const char alnum[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

String generate_random_sequence(uint32_t len)
{
    auto seed = std::random_device()();
    std::mt19937 g(seed);
    std::uniform_int_distribution<> d(1, sizeof(alnum) - 1);
    String seq(len, 0);
    while (len)
        seq[--len] = alnum[d(g) - 1];
    return seq;
};

String hash_to_string(const String &hash)
{
    return hash_to_string((uint8_t *)hash.c_str(), hash.size());
}

String hash_to_string(const uint8_t *hash, size_t hash_size)
{
    String s;
    for (uint32_t i = 0; i < hash_size; i++)
    {
        s += alnum[(hash[i] & 0xF0) >> 4];
        s += alnum[(hash[i] & 0x0F) >> 0];
    }
    return s;
}

String shorten_hash(const String &data)
{
    if (data.size() <= CPPAN_CONFIG_HASH_SHORT_LENGTH)
        return data;
    return data.substr(0, CPPAN_CONFIG_HASH_SHORT_LENGTH);
}

String sha1(const String &data)
{
    uint8_t hash[EVP_MAX_MD_SIZE];
    uint32_t hash_size;
    EVP_Digest(data.data(), data.size(), hash, &hash_size, EVP_sha1(), nullptr);
    return hash_to_string(hash, hash_size);
}

String sha256(const String &data)
{
    uint8_t hash[EVP_MAX_MD_SIZE];
    uint32_t hash_size;
    EVP_Digest(data.data(), data.size(), hash, &hash_size, EVP_sha256(), nullptr);
    return hash_to_string(hash, hash_size);
}

String sha256_short(const String &data)
{
    return shorten_hash(sha256(data));
}

String hash_config(const String &c)
{
    return sha256_short(c);
}
