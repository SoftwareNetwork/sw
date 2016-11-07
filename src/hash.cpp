/*
 * Copyright (c) 2016, Egor Pugin
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     1. Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *     3. Neither the name of the copyright holder nor the names of
 *        its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "hash.h"

#include <random>

#include <boost/algorithm/string.hpp>
#include <openssl/evp.h>

String generate_random_sequence(uint32_t len)
{
    auto seed = std::random_device()();
    std::mt19937 g(seed);
    std::uniform_int_distribution<> d(0, 127);
    String user_session(len, 0);
    while (len)
    {
        char c;
        do c = (char)d(g);
        while (!isalnum(c));
        user_session[--len] = c;
    }
    return user_session;
};

String hash_to_string(const String &hash)
{
    return hash_to_string((uint8_t *)hash.c_str(), hash.size());
}

String hash_to_string(const uint8_t *hash, size_t hash_size)
{
    static auto alnum16 = "0123456789abcdef";

    String s;
    for (uint32_t i = 0; i < hash_size; i++)
    {
        s += alnum16[(hash[i] & 0xF0) >> 4];
        s += alnum16[(hash[i] & 0x0F) >> 0];
    }
    return s;
}

String shorten_hash(const String &data)
{
    if (data.size() <= 8)
        return data;
    return data.substr(0, 8);
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
