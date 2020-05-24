// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <primitives/filesystem.h>

SW_CLIENT_COMMON_API
void self_upgrade(const String &progname);

void self_upgrade_copy(const path &dst);
