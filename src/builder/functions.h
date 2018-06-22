// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <primitives/filesystem.h>

namespace sw
{

SW_BUILDER_API
void fileWriteOnce(const path &fn, const String &content, const path &lock_dir);

SW_BUILDER_API
void fileWriteSafe(const path &fn, const String &content, const path &lock_dir);

SW_BUILDER_API
void replaceInFileOnce(const path &fn, const String &from, const String &to, const path &lock_dir);

SW_BUILDER_API
void pushFrontToFileOnce(const path &fn, const String &text, const path &lock_dir);

SW_BUILDER_API
void pushBackToFileOnce(const path &fn, const String &text, const path &lock_dir);

}
