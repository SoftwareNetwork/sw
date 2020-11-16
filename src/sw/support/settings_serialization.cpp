// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <boost/serialization/access.hpp>
#include <boost/serialization/split_member.hpp>

#include "settings.h"

#include <boost/serialization/map.hpp>
#include <boost/serialization/vector.hpp>
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

PackageSettings loadSettings(const path &archive_fn, int type)
{
    return deserialize<PackageSettings>(archive_fn, type);
}

void saveSettings(const path &archive_fn, const PackageSettings &s, int type)
{
    serialize(archive_fn, s, type);
}

} // namespace sw
