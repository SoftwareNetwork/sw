// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#include "input_database.h"

#include "specification.h"

#include <db_inputs.h>
#include "inserts.h"
#include <sqlpp11/sqlite3/connection.h>
#include <sqlpp11/sqlite3/sqlite3.h>
#include <sqlpp11/sqlpp11.h>

#include <string.h> // memcpy

namespace sw
{

InputDatabase::InputDatabase(const path &p)
    : Database(p, inputs_db_schema)
{
}

size_t InputDatabase::getFileHash(const path &p) const
{
    const ::db::inputs::File file{};

    bool update_db = false;
    auto lwt = fs::last_write_time(p);
    auto np = to_string(normalize_path(p));

    auto q = (*db)(
        select(file.hash, file.lastWriteTime)
        .from(file)
        .where(file.path == np));
    if (!q.empty())
    {
        if (memcmp(q.front().lastWriteTime.value().data(), &lwt, sizeof(lwt)) == 0)
            return q.front().hash.value();
        update_db = true;
    }

    auto c = read_file(p);
    auto h = std::hash<String>()(c);

    std::vector<uint8_t> lwtdata(sizeof(lwt));
    memcpy(lwtdata.data(), &lwt, lwtdata.size());
    if (update_db)
    {
        (*db)(update(file).set(
            file.hash = h,
            file.lastWriteTime = lwtdata
        ).where(file.path == np));
    }
    else
    {
        (*db)(insert_into(file).set(
            file.path = np,
            file.hash = h,
            file.lastWriteTime = lwtdata
        ));
    }

    return h;
}

} // namespace sw
