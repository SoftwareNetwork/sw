// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "configuration.h"

#include <boost/algorithm/string.hpp>

namespace sw
{

void addConfigElement(String &c, const String &e)
{
    boost::replace_all(c, "-", "_");
    c += e + "-";
}

ConfigurationBase ConfigurationBase::operator|(const ConfigurationBase &rhs) const
{
    auto tmp = *this;
    tmp |= rhs;
    return tmp;
}

ConfigurationBase &ConfigurationBase::operator|=(const ConfigurationBase &rhs)
{
    apply(rhs);
    return *this;
}

void ConfigurationBase::apply(const ConfigurationBase &rhs)
{
    // handle package ranges
    for (auto &[p, c] : rhs.Settings)
    {
        // provide more intelligent way of merging
        Settings[p].insert(c.begin(), c.end());
    }
}

}
