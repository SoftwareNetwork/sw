// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "input_database.h"

#include "driver.h"

#include <db_inputs.h>
#include "inserts.h"
#include <sqlpp11/sqlite3/connection.h>
#include <sqlpp11/sqlite3/sqlite3.h>
#include <sqlpp11/sqlpp11.h>

namespace sw
{

InputDatabase::InputDatabase(const path &p)
    : Database(p, inputs_db_schema)
{
}

size_t InputDatabase::addInputFile(const path &p) const
{
    const ::db::inputs::File file{};

    auto set_input = [&]()
    {
        Specification s;
        s.addFile(p, read_file(p));
        auto h = s.getHash();

        // spec may contain many files
        for (auto &[f,_] : s.files)
        {
            auto lwt = fs::last_write_time(f);
            std::vector<uint8_t> lwtdata(sizeof(lwt));
            memcpy(lwtdata.data(), &lwt, lwtdata.size());

            (*db)(insert_into(file).set(
                file.path = normalize_path(f),
                file.hash = h,
                file.lastWriteTime = lwtdata
            ));
        }

        return h;
    };

    auto q = (*db)(
        select(file.fileId, file.hash, file.lastWriteTime)
        .from(file)
        .where(file.path == normalize_path(p)));
    if (q.empty())
        return set_input();

    bool ok = true;
    auto q2 = (*db)(
        select(file.fileId, file.path, file.lastWriteTime)
        .from(file)
        .where(file.hash == q.front().hash.value()));
    for (const auto &row : q2)
    {
        if (!fs::exists(row.path.value()))
        {
            ok = false;
            break;
        }
        auto lwt = fs::last_write_time(row.path.value());
        ok &= memcmp(row.lastWriteTime.value().data(), &lwt, sizeof(lwt)) == 0;
        if (!ok)
            break;
    }
    if (ok)
        return q.front().hash.value();
    else
    {
        // remove old first
        for (const auto &row : q2)
            (*db)(remove_from(file).where(file.fileId == row.fileId));
        return set_input();
    }
}

void InputDatabase::setupInput(Input &i) const
{
    if (i.getType() == InputType::Directory)
    {
        // set hash by path
        i.setHash(std::hash<path>()(i.getPath()));
        return;
    }

    const ::db::inputs::File file{};

    auto set_input = [&]()
    {
        auto spec = i.getSpecification();
        auto h = spec->getHash();
        i.setHash(h);

        // spec may contain many files
        for (auto &[f,_] : spec->files)
        {
            auto lwt = fs::last_write_time(f);
            std::vector<uint8_t> lwtdata(sizeof(lwt));
            memcpy(lwtdata.data(), &lwt, lwtdata.size());

            (*db)(insert_into(file).set(
                file.path = normalize_path(f),
                file.hash = h,
                file.lastWriteTime = lwtdata
            ));
        }
    };

    auto q = (*db)(
        select(file.fileId, file.hash, file.lastWriteTime)
        .from(file)
        .where(file.path == normalize_path(i.getPath())));
    if (q.empty())
    {
        set_input();
        return;
    }

    bool ok = true;
    auto q2 = (*db)(
        select(file.fileId, file.path, file.lastWriteTime)
        .from(file)
        .where(file.hash == q.front().hash.value()));
    for (const auto &row : q2)
    {
        if (!fs::exists(row.path.value()))
        {
            ok = false;
            break;
        }
        auto lwt = fs::last_write_time(row.path.value());
        ok &= memcmp(row.lastWriteTime.value().data(), &lwt, sizeof(lwt)) == 0;
        if (!ok)
            break;
    }
    if (ok)
        i.setHash(q.front().hash.value());
    else
    {
        // remove old first
        for (const auto &row : q2)
            (*db)(remove_from(file).where(file.fileId == row.fileId));
        set_input();
    }
}

} // namespace sw
