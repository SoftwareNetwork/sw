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

#include "file_storage.h"

#include "file.h"
#include "sw_context.h"

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "file_storage");

namespace sw
{

void FileStorage::clear()
{
    files.clear();
}

void FileStorage::reset()
{
    for (const auto &[k, f] : files)
        f.reset();
}

FileData &FileStorage::registerFile(const path &in_f)
{
    auto p = normalize_path(in_f);
    auto d = files.insert(p);
    if (d.second)
        d.first->refresh(in_f);
    return *d.first;
}

}
