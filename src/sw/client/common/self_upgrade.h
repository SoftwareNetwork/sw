// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <primitives/filesystem.h>

SW_CLIENT_COMMON_API
void self_upgrade(const String &progname);

void self_upgrade_copy(const path &dst);
