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

#pragma once

#include "cppan_string.h"
#include "filesystem.h"

#define CPPAN_CONFIG_HASH_METHOD "SHA256"
#define CPPAN_CONFIG_HASH_SHORT_LENGTH 8

String generate_random_sequence(uint32_t len);
String hash_to_string(const uint8_t *hash, size_t hash_size);
String hash_to_string(const String &hash);

String md5(const String &data);
String md5(const path &fn);
String sha1(const String &data);
String sha256(const String &data);
String sha256(const path &fn);
String sha256_short(const String &data);
String sha3_256(const String &data);

String strong_file_hash(const path &fn);

String shorten_hash(const String &data);

String hash_config(const String &c);
