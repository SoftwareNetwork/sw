// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

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
    // v0 - initial
    // v1, v2 - see git history
    // v3 - detect appleclang properly
    fn = in_fn.parent_path() / in_fn.stem() += ".3.json";
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
        auto s = to_string(normalize_path(p));
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

void ProgramVersionStorage::addVersion(const path &p, const PackageVersion &v, const String &output)
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
