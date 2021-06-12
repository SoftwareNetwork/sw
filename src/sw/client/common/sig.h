// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#include <primitives/filesystem.h>

//void ds_sign_file(const path &fn, const path &pkey_fn);
//void ds_verify_file(const path &fn, const path &sigfn, const String &pubkey);
//void ds_verify_file(const path &fn, const path &sigfn, const path &pubkey_fn);
void ds_verify_sw_file(const path &fn, const String &algo, const String &sig);
