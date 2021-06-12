// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2019-2020 Egor Pugin <egor.pugin@gmail.com>

#include "specification.h"

#include "input_database.h"

#include <sw/support/hash.h>

namespace sw
{

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
        if (f.absolute_path.empty())
        {
            // for virtual files
            hash_combine(h, std::hash<String>()(f.getContents()));
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
        return to_string(normalize_path(dir));
    for (auto &[_, f] : this->files.getData())
        return to_string(normalize_path(f.absolute_path));
    return "<empty specification>";
}

void Specification::read()
{
    files.read();
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
