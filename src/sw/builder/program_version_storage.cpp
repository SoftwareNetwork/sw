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
    String s;
    std::ifstream ifile(fn);
    while (ifile)
    {
        ifile >> p;
        if (!ifile)
            break;
        ifile >> s;
        //if (!File(p, service_fs).isChanged())
            //versions[p] = s;
    }
}

ProgramVersionStorage::~ProgramVersionStorage()
{
    std::ofstream ofile(fn);
    for (auto &[p, v] : std::map{ versions.begin(), versions.end() })
        ofile << p << " " << v.toString() << "\n";
}

}
