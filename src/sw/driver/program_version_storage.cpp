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

#include <base64.h>
#include <nlohmann/json.hpp>

#include <fstream>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "pvs");

namespace sw
{

ProgramVersionStorage::ProgramVersionStorage(const path &in_fn)
{
    fn = in_fn.parent_path() / in_fn.stem() += ".2.json";
    if (!fs::exists(fn))
        return;

    try
    {
        auto j = nlohmann::json::parse(read_file(fn));
        auto &jd = j["data"];
        for (auto &[prog, d] : jd.items())
        {
            String o = d["output"];
            o = base64_decode(o);
            String v = d["version"];
            time_t t = d["lwt"];
            path p = prog;
            if (!fs::exists(p))
                continue;
            auto lwt = fs::last_write_time(p);
            if (file_time_type2time_t(lwt) <= t)
                versions[p] = {o,v,lwt};
        }
    }
    // uncomment when base64 will be fixed
    //catch (std::exception &)
    catch (...)
    {
        std::error_code ec;
        fs::remove(fn, ec);
    }
}

ProgramVersionStorage::~ProgramVersionStorage()
{
    nlohmann::json j;
    j["schema"]["version"] = 1;
    auto &jd = j["data"];
    for (auto &[p, v] : versions)
    {
        auto s = normalize_path(p);
        jd[s]["output"] = base64_encode(v.output);
        jd[s]["version"] = v.v.toString();
        jd[s]["lwt"] = file_time_type2time_t(v.t);
    }

    bool e = fs::exists(fn);
    try
    {
        write_file(fn, j.dump());
    }
    catch (std::exception &ex)
    {
        if (!e)
            LOG_WARN(logger, "pvs write error: " << ex.what());
        else
        {
            std::error_code ec;
            fs::remove(fn, ec);
        }
    }
}

void ProgramVersionStorage::addVersion(const path &p, const Version &v, const String &output)
{
    versions[normalize_path(p)] = {output, v, fs::last_write_time(p)};
}

ProgramVersionStorage &getVersionStorage(const SwManagerContext &swctx)
{
    // maybe store program db in .sw?
    static ProgramVersionStorage pvs(swctx.getLocalStorage().storage_dir_tmp / "db" / "program_versions.txt");
    return pvs;
}

}
