// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "version.h"

#include <iomanip>
#include <regex>
#include <sstream>

namespace sw
{

std::optional<Version> VersionRange::getMinSatisfyingVersion(const VersionSet &s) const
{
    // add policies?

    if (!s.empty_releases())
    {
        for (auto &v : s.releases())
        {
            if (hasVersion(v))
                return v;
        }
    }

    return Base::getMinSatisfyingVersion(s);
}

std::optional<Version> VersionRange::getMaxSatisfyingVersion(const VersionSet &s) const
{
    // add policies?

    if (!s.empty_releases())
    {
        for (auto i = s.rbegin_releases(); i != s.rend_releases(); ++i)
        {
            if (hasVersion(*i))
                return *i;
        }
    }

    return Base::getMaxSatisfyingVersion(s);
}

}
