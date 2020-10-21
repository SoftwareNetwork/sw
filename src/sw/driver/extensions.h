// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <primitives/string.h>

namespace sw
{

SW_DRIVER_CPP_API
const StringSet &getCppHeaderFileExtensions();

SW_DRIVER_CPP_API
const StringSet &getCppSourceFileExtensions();

SW_DRIVER_CPP_API
bool isCppHeaderFileExtension(const String &);

SW_DRIVER_CPP_API
bool isCppSourceFileExtensions(const String &);

}
