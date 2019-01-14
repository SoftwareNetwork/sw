// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cppan_version.h"

//#include "resolver.h"

#include <iomanip>
#include <regex>
#include <sstream>

/*int64_t Version::toNumber(int digits) const
{
    if (!Branch.empty())
        return 0;

    if (digits != 2 && digits != 4)
        throw SW_RUNTIME_ERROR("digits must be 2 or 4");

    auto shift = 4 * digits;

    // hex
    int64_t r = 0;
    r |= (int64_t)Major << shift;
    r |= (int64_t)Minor << shift;
    r |= (int64_t)Patch << shift;

    /*
    // decimal
    int64_t m = 1;
    int64_t r = 0;
    r += (int64_t)Patch * m;
    for (int i = 0; i < digits; i++)
        m *= 10;
    r += (int64_t)Minor * m;
    for (int i = 0; i < digits; i++)
        m *= 10;
    r += (int64_t)Major * m;

    return r;
}*/

namespace sw
{

/// resolve from db
std::optional<Version> VersionRange::getMinSatisfyingVersion() const
{
    return {};
}

/// resolve from db
std::optional<Version> VersionRange::getMaxSatisfyingVersion() const
{
    return {};
}

}
