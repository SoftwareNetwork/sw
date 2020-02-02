// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "settings.h"

#include <boost/serialization/map.hpp>
#include <primitives/exceptions.h>

#include <fstream>

// last
#include <sw/support/serialization.h>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4005) // warning C4005: 'XXX': macro redefinition
#endif

namespace sw
{

TargetSettings loadSettings(const path &archive_fn, int type)
{
    return deserialize<TargetSettings>(archive_fn, type);
}

void saveSettings(const path &archive_fn, const TargetSettings &s, int type)
{
    serialize(archive_fn, s, type);
}

} // namespace sw
