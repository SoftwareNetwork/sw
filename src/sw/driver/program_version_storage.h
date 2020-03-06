// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <sw/manager/version.h>

namespace sw
{

struct SwManagerContext;

struct ProgramVersionStorage
{
    struct ProgramInfo
    {
        Version v;
        fs::file_time_type t;

        operator Version&() { return v; }
    };

    path fn;
    std::unordered_map<path, ProgramInfo> versions;

    ProgramVersionStorage(const path &fn);
    ~ProgramVersionStorage();

    void addVersion(const path &p, const Version &v)
    {
        versions[p] = {v,fs::last_write_time(p)};
    }
};

ProgramVersionStorage &getVersionStorage(const SwManagerContext &);

}
