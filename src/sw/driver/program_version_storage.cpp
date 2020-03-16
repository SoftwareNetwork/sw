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

#include "program_version_storage.h"

#include <sw/builder/file.h>
#include <sw/manager/sw_context.h>
#include <sw/manager/storage.h>

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
        if (!fs::exists(p))
            continue;
        auto lwt = fs::last_write_time(p);
        if (t && file_time_type2time_t(lwt) <= t)
            versions[p] = {v,lwt};
    }
}

ProgramVersionStorage::~ProgramVersionStorage()
{
    std::ofstream ofile(fn);
    for (auto &[p, v] : std::map<path, ProgramInfo>(versions.begin(), versions.end()))
        ofile << p << " " << v.v.toString() << " " << file_time_type2time_t(v.t) << "\n";
}

ProgramVersionStorage &getVersionStorage(const SwManagerContext &swctx)
{
    // maybe store program db in .sw?
    static ProgramVersionStorage pvs(swctx.getLocalStorage().storage_dir_tmp / "db" / "program_versions.txt");
    return pvs;
}

}
