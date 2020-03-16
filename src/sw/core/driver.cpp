/*
 * SW - Build System and Package Manager
 * Copyright (C) 2019-2020 Egor Pugin
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

#include "driver.h"

#include <sw/support/hash.h>

namespace sw
{

IDriver::~IDriver()
{
}

void Specification::addFile(const path &relative_path, const String &contents)
{
    files[relative_path] = contents;
}

int64_t Specification::getHash() const
{
    size_t h = 0;
    if (files.size() != 1)
        SW_UNIMPLEMENTED;
    //for (auto &[f, s] : files)
        //hash_combine(h, s);
    h = std::hash<String>()(files.begin()->second);
    return h;
}

} // namespace sw
