/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2020 Egor Pugin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "configuration.h"

#include <boost/algorithm/string.hpp>

namespace sw
{

void addConfigElement(String &c, const String &e)
{
    if (e.empty())
        return;
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
