/*
 * Copyright (c) 2016, Egor Pugin
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     1. Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *     3. Neither the name of the copyright holder nor the names of
 *        its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "access_table.h"

#include "common.h"

#include <boost/interprocess/sync/file_lock.hpp>

#include <unordered_map>

const String stamp =
#include <stamp.h>
;

struct AccessData
{
    bool initialized = false;
    path root_dir;
    path root_file;
    path lock_file;
    std::unordered_map<path, time_t> stamps;
    int refs = 0;

    void init(const path &rd)
    {
        if (initialized)
            return;

        root_dir = rd / "stamps";
        if (!fs::exists(root_dir))
            fs::create_directories(root_dir);

        root_file = root_dir / stamp;
        lock_file = root_dir / (stamp + ".lock");

        if (!fs::exists(lock_file))
            std::ofstream(lock_file.string());

        initialized = true;
    }

    void load()
    {
        if (!fs::exists(root_file))
            return;
        if (refs++ > 0)
            return;

        boost::interprocess::file_lock lck(lock_file.string().c_str());
        lck.lock_sharable();

        path p;
        time_t t;
        std::ifstream ifile(root_file.string());
        while (ifile >> p >> t)
            stamps[p] = t;

        lck.unlock_sharable();
    }

    void save()
    {
        if (--refs > 0)
            return;

        boost::interprocess::file_lock lck(lock_file.string().c_str());
        lck.lock();

        std::ofstream ofile(root_file.string());
        for (auto &s : stamps)
            ofile << s.first << " " << s.second << "\n";

        lck.unlock();
    }
};

static AccessData data;

AccessTable::AccessTable(const path &cfg_dir)
    : root_dir(cfg_dir.parent_path())
{
    data.init(cfg_dir);
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
    if (!isUnderRoot(p))
        return true;
    return fs::last_write_time(p) != data.stamps[p];
}

void AccessTable::update_contents(const path &p, const String &s) const
{
    write_file_if_different(p, s);
    data.stamps[p] = fs::last_write_time(p);
}

void AccessTable::write_if_older(const path &p, const String &s) const
{
    if (!isUnderRoot(p))
    {
        write_file_if_different(p, s);
        return;
    }
    if (must_update_contents(p))
        update_contents(p, s);
}

void AccessTable::clear() const
{
    data.stamps.clear();
}

bool AccessTable::isUnderRoot(path p) const
{
    return isUnderRoot(p, root_dir);
}

bool AccessTable::isUnderRoot(path p, const path &root_dir)
{
    while (!p.empty())
    {
        if (p == root_dir)
            return true;
        p = p.parent_path();
    }
    return false;
}

void AccessTable::remove(const path &p) const
{
    std::set<path> rm;
    for (auto &s : data.stamps)
    {
        if (isUnderRoot(s.first, p))
            rm.insert(s.first);
    }
    for (auto &s : rm)
        data.stamps.erase(s);
}
