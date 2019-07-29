// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "program_version_storage.h"

#include "file.h"

#include <fstream>

namespace sw
{

ProgramVersionStorage::ProgramVersionStorage(const path &fn)
    : fn(fn)
{
    path p;
    String v;
    time_t t;
    std::ifstream ifile(fn);
    while (1)
    {
        ifile >> p;
        if (!ifile)
            break;
        ifile >> v;
        ifile >> t;
        auto lwt = fs::last_write_time(p).time_since_epoch().count();
        if (t && lwt <= t)
            versions[p] = {v,lwt};
    }
}

ProgramVersionStorage::~ProgramVersionStorage()
{
    std::ofstream ofile(fn);
    for (auto &[p, v] : std::map{ versions.begin(), versions.end() })
        ofile << p << " " << v.v.toString() << " " << v.t << "\n";
}

}
