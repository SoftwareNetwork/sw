// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <primitives/hash.h>
#include <primitives/hash_combine.h>

SW_SUPPORT_API
String shorten_hash(const String &data);

SW_SUPPORT_API
String sha256_short(const String &data);

SW_SUPPORT_API
String hash_config(const String &c);

SW_SUPPORT_API
String get_file_hash(const path &fn);

SW_SUPPORT_API
bool check_file_hash(const path &fn, const String &hash);

SW_SUPPORT_API
String getCurrentModuleNameHash();

SW_SUPPORT_API
String getCurrentModuleHash();
