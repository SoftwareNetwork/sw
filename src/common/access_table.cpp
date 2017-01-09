/*
 * Copyright (C) 2016-2017, Egor Pugin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "access_table.h"

#include "cppan_string.h"
#include "database.h"
#include "directories.h"
#include "lock.h"
#include "stamp.h"

#include <unordered_map>

struct AccessData
{
    Stamps stamps;
    bool do_not_update = false;
    int refs = 0;

    void load()
    {
        if (refs++ > 0)
            return;

        stamps = getServiceDatabase().getFileStamps();
    }

    void save()
    {
        if (--refs > 0)
            return;

        getServiceDatabase().setFileStamps(stamps);
    }

    void clear()
    {
        stamps.clear();
        getServiceDatabase().clearFileStamps();
    }
};

static AccessData data;

AccessTable::AccessTable()
{
    data.load();
}

AccessTable::~AccessTable()
{
    data.save();
}

bool AccessTable::must_update_contents(const path &p) const
{
    if (!fs::exists(p))
        return true;
    if (data.do_not_update)
        return false;
    if (!is_under_root(p, directories.storage_dir_etc))
        return true;
    return fs::last_write_time(p) != data.stamps[p];
}

bool AccessTable::updates_disabled() const
{
    return data.do_not_update;
}

void AccessTable::update_contents(const path &p, const String &s) const
{
    write_file_if_different(p, s);
    data.stamps[p] = fs::last_write_time(p);
}

void AccessTable::write_if_older(const path &p, const String &s) const
{
    if (!is_under_root(p, directories.storage_dir_etc))
    {
        write_file_if_different(p, s);
        return;
    }
    if (must_update_contents(p))
        update_contents(p, s);
}

void AccessTable::clear() const
{
    data.clear();
}

void AccessTable::remove(const path &p) const
{
    std::set<path> rm;
    for (auto &s : data.stamps)
    {
        if (is_under_root(s.first, p))
            rm.insert(s.first);
    }
    for (auto &s : rm)
        data.stamps.erase(s);
}

void AccessTable::do_not_update_files(bool v)
{
    data.do_not_update = v;
}
