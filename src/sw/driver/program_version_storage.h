// SPDX-License-Identifier: AGPL-3.0-only
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
        PackageVersion v;
        fs::file_time_type t;

        operator PackageVersion&() { return v; }
    };

    path fn;
    std::map<path, ProgramInfo> versions;

    ProgramVersionStorage(const path &fn);
    ~ProgramVersionStorage();

    void addVersion(const path &p, const PackageVersion &v, const String &output);
};

ProgramVersionStorage &getVersionStorage(const SwManagerContext &);

}
