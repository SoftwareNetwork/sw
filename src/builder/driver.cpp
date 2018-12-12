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

bool Driver::hasConfig(const path &dir) const
{
    return fs::exists(dir / getConfigFilename());
}

PackageScriptPtr Driver::fetch_and_load(const path &file_or_dir, bool parallel) const
{
    fetch(file_or_dir, parallel);
    return load(file_or_dir);
}

bool Driver::execute(const path &file_or_dir) const
{
    if (auto s = load(file_or_dir); s)
        return s->execute();
    return false;
}

} // namespace sw
