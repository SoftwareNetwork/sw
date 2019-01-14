// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <sw/builder/driver.h>

namespace sw
{

Drivers &getDrivers()
{
    static Drivers drivers;
    return drivers;
}

std::optional<path> Driver::findConfig(const path &dir) const
{
    for (auto &fn : getAvailableFrontends())
        if (fs::exists(dir / fn))
            return dir / fn;
    return {};
}

std::optional<String> Driver::readConfig(const path &file_or_dir) const
{
    auto f = findConfig(file_or_dir);
    if (!f)
        return {};
    return read_file(f.value());
}

PackageScriptPtr Driver::fetch_and_load(const path &file_or_dir, const FetchOptions &opts, bool parallel) const
{
    fetch(file_or_dir, opts, parallel);
    return load(file_or_dir);
}

bool Driver::execute(const path &file_or_dir) const
{
    if (auto s = load(file_or_dir); s)
        return s->execute();
    return false;
}

} // namespace sw
