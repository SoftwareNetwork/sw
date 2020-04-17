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

#include "specification.h"

#include "input_database.h"

#include <sw/support/hash.h>

namespace sw
{

void SpecificationFiles::addFile(const path &relative_path, const path &abspath, const std::optional<String> &contents)
{
    if (relative_path.is_absolute())
        throw SW_RUNTIME_ERROR("Not a relative path: " + normalize_path(relative_path));
    data[relative_path] = { abspath, contents };
}

fs::file_time_type SpecificationFiles::getLastWriteTime() const
{
    auto lwt = fs::file_time_type::min();
    for (auto &[_, f] : data)
        lwt = std::max(lwt, fs::last_write_time(f.absolute_path));
    return lwt;
}

Specification::Specification(const SpecificationFiles &files)
    : files(files)
{
}

Specification::Specification(const path &dir)
    : dir(dir)
{
}

size_t Specification::getHash(const InputDatabase &db) const
{
    if (!dir.empty())
        return std::hash<path>()(dir);

    size_t h = 0;
    for (auto &[rel, f] : files.getData())
    {
        if (f.contents)
        {
            // for virtual files
            hash_combine(h, std::hash<String>()(*f.contents));
            continue;
        }

        hash_combine(h, db.getFileHash(f.absolute_path));
    }
    return h;
}

Files Specification::getFiles() const
{
    Files files;
    for (auto &[_, f] : this->files.getData())
        files.insert(f.absolute_path);
    return files;
}

bool Specification::isOutdated(const fs::file_time_type &t) const
{
    if (!dir.empty())
        return true;
    return t < files.getLastWriteTime();
}

String Specification::getName() const
{
    if (!dir.empty())
        return normalize_path(dir);
    for (auto &[_, f] : this->files.getData())
        return normalize_path(f.absolute_path);
    SW_UNIMPLEMENTED;
}

/*const String &Specification::getFileContents(const path &p)
{
    auto it = files.find(p);
    if (it == files.end())
        throw SW_RUNTIME_ERROR("No such file " + p.u8string());
    if (!it->second.contents)
        it->second.contents = read_file(p);
    return *it->second.contents;
}

const String &Specification::getFileContents(const path &p) const
{
    auto it = files.find(p);
    if (it == files.end())
        throw SW_RUNTIME_ERROR("No such file " + p.u8string());
    if (!it->second.contents)
        throw SW_RUNTIME_ERROR("No contents was set for file " + it->second.absolute_path.u8string() + " (" + p.u8string() + ")");
    return *it->second.contents;
}*/

} // namespace sw
