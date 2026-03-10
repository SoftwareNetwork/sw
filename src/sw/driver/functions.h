// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <primitives/constants.h>
#include <primitives/filesystem.h>

namespace sw
{

using patch_data_type = std::map<path, bool>;

SW_DRIVER_CPP_API
void writeFileOnce(const path &fn, const String &content, const path &lock_dir);

SW_DRIVER_CPP_API
void writeFileSafe(const path &fn, const String &content, const path &lock_dir);

SW_DRIVER_CPP_API
void replaceInFileOnce(patch_data_type &, const path &fn, const String &from, const String &to, const path &lock_dir);

SW_DRIVER_CPP_API
void pushFrontToFileOnce(patch_data_type &, const path &fn, const String &text, const path &lock_dir);

SW_DRIVER_CPP_API
void pushBackToFileOnce(patch_data_type &, const path &fn, const String &text, const path &lock_dir);

SW_DRIVER_CPP_API
bool patch(const path &fn, const String &text, const path &lock_dir);

SW_DRIVER_CPP_API
void downloadFile(const String &url, const path &fn, int64_t file_size_limit = 1_MB);

SW_DRIVER_CPP_API
path getProgramLocation();

}
