// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <primitives/http.h>

SW_SUPPORT_API
bool isValidSourceUrl(const String &url);

SW_SUPPORT_API
void checkSourceUrl(const String &url);
