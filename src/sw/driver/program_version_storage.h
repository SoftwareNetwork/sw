// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <sw/support/version.h>

namespace sw
{

struct SwManagerContext;

struct ProgramVersionStorage
{
    struct ProgramInfo
    {
        String output;
        Version v;
        fs::file_time_type t;

        operator Version&() { return v; }
    };

    path fn;
    std::map<path, ProgramInfo> versions;

    ProgramVersionStorage(const path &fn);
    ~ProgramVersionStorage();

    void addVersion(const path &p, const Version &v, const String &output);
};

ProgramVersionStorage &getVersionStorage(const SwManagerContext &);

}
