// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "db_file.h"

#include <directories.h>
//#include <target.h>

#include <sqlite_database.h>
#include <sqlite3.h>

void save_from_memory_to_file(const path &fn, sqlite3 *db);

#include <primitives/context.h>
#include <primitives/date_time.h>
#include <primitives/debug.h>
#include <primitives/lock.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "db_file");

namespace sw
{

static path getDir()
{
    return getUserDirectories().storage_dir_tmp / "db";
}

static void load(FileStorage &fs, const path &fn, ConcurrentHashMap<path, FileRecord> &files, std::unordered_map<int64_t, std::unordered_set<int64_t>> &deps)
{
    ScopedShareableFileLock lk(fn);

    FILE *fp = primitives::filesystem::fopen(fn, "rb");
    if (!fp)
    {
        if (fs::exists(fn))
            throw std::runtime_error("Cannot open file: " + fn.u8string());
        return;
    }
    while (!feof(fp))
    {
        int64_t h;
        fread(&h, sizeof(h), 1, fp);

        if (feof(fp))
            break;

        size_t sz;
        fread(&sz, sizeof(sz), 1, fp);

        String p(sz, 0);
        fread(&p[0], sz, 1, fp);

        auto kv = files.insert(h);
        kv.first->file = p;
        kv.first->data = fs.registerFile(p)->data;

        decltype(kv.first->data->last_write_time) lwt;
        fread(&lwt, sizeof(kv.first->data->last_write_time), 1, fp);

        /*sz = 0;
        fread(&sz, sizeof(kv.first->data->size), 1, fp);

        uint64_t flags;
        fread(&flags, sizeof(flags), 1, fp);*/

        if (kv.first->data->last_write_time < lwt)
        {
            kv.first->data->last_write_time = lwt;
            //kv.first->data->size = sz;
            //kv.first->data->flags = flags;
        }

        size_t n;
        fread(&n, sizeof(n), 1, fp);

        for (size_t i = 0; i < n; i++)
        {
            int64_t h2;
            fread(&h2, sizeof(h2), 1, fp);

            deps[h].insert(h2);
        }
    }
    fclose(fp);
}

Db &getDb()
{
    static std::unique_ptr<Db> db = std::make_unique<FileDb>();
    return *db;
}

void FileDb::load(FileStorage &fs, ConcurrentHashMap<path, FileRecord> &files) const
{
    std::unordered_map<int64_t, std::unordered_set<int64_t>> deps;

    sw::load(fs, getDir() += "." + fs.config + ".files", files, deps);
    sw::load(fs, getFilesLogFileName(fs.config), files, deps);
    error_code ec;
    fs::remove(getFilesLogFileName(fs.config), ec);

    for (auto &[k, v] : deps)
    {
        for (auto &h2 : v)
        {
            if (!h2)
                continue;
            auto k2 = &files[h2];
            if (k2)
                files[k].implicit_dependencies.insert({ k2->file, k2 });
        }
    }
}

void FileDb::save(FileStorage &fs, ConcurrentHashMap<path, FileRecord> &files) const
{
    auto f = getDir() += "." + fs.config + ".files";

    // first, we load current copy of files
    ConcurrentHashMap<path, FileRecord> old;
    load(fs, old);

    // compare and renew our (actually any) copy
    for (auto i = old.getIterator(); i.isValid(); i.next())
    {
        auto &f = *i.getValue();
        auto[ptr, inserted] = files.insert(f.file, f);
        if (!inserted && f.data && *ptr < f)
            *ptr = f;
    }

    ScopedTime t;
    {
        // lock and write
        ScopedFileLock lk(f);
        ScopedFile fp(f, "wb");
        std::vector<uint8_t> v;
        for (auto i = files.getIterator(); i.isValid(); i.next())
        {
            auto &f = *i.getValue();
            if (!f.data)
                continue;

            //DEBUG_BREAK_IF_PATH_HAS(f.file, "gettext-runtime/intl/vasnprintf.c");

            //f.isChanged(false); // update file info
            write(v, f);
            fwrite(&v[0], v.size(), 1, fp.getHandle());
        }
    }
    /*LOG_INFO(logger, "save to file db time: " << t.getTimeFloat() << " s.");

    ScopedTime t2;
    {
        int r;

#define CHECK_RC(rc, ec) if (r != ec) return

        sqlite3 *db;
        r = sqlite3_open(":memory:", &db);
        CHECK_RC(r, SQLITE_OK);

        r = sqlite3_exec(db, R"xxx(
CREATE TABLE "file" (
    "hash" INTEGER,
    "path" TEXT,
    "last_write_time" INTEGER,
    "size" INTEGER,
    "flags" INTEGER
);

CREATE TABLE "file_dependency" (
    "hash" INTEGER,
    "dependency_hash" INTEGER
);
)xxx", 0, 0, 0);
        CHECK_RC(r, SQLITE_OK);

        r = sqlite3_exec(db, "BEGIN", 0, 0, 0);
        CHECK_RC(r, SQLITE_OK);

        sqlite3_stmt *sf;
        r = sqlite3_prepare_v2(db, "INSERT INTO file (path, last_write_time, hash) VALUES (?,?,?)", -1, &sf, 0);
        CHECK_RC(r, SQLITE_OK);

        sqlite3_stmt *sd;
        r = sqlite3_prepare_v2(db, "INSERT INTO file_dependency VALUES (?,?)", -1, &sd, 0);
        CHECK_RC(r, SQLITE_OK);

        for (auto i = files.getIterator(); i.isValid(); i.next())
        {
            auto &f = *i.getValue();
            if (!f.data)
                continue;

            auto h1 = std::hash<path>()(f.file);
            auto s = normalize_path(f.file);
            sqlite3_bind_text(sf, 1, s.c_str(), s.size() + 1, 0);
            sqlite3_bind_int64(sf, 2, f.data->last_write_time.time_since_epoch().count());
            //sqlite3_bind_int64(sf, 3, f.data->size);
            sqlite3_bind_int64(sf, 3, h1);

            r = sqlite3_step(sf);
            CHECK_RC(r, SQLITE_DONE);

            r = sqlite3_reset(sf);
            CHECK_RC(r, SQLITE_OK);

            for (auto &[f, d] : f.implicit_dependencies)
            {
                sqlite3_bind_int64(sd, 1, h1);
                sqlite3_bind_int64(sd, 2, std::hash<path>()(d->file));

                r = sqlite3_step(sd);
                CHECK_RC(r, SQLITE_DONE);

                r = sqlite3_reset(sd);
                CHECK_RC(r, SQLITE_OK);
            }
        }

        r = sqlite3_finalize(sf);
        CHECK_RC(r, SQLITE_OK);

        r = sqlite3_finalize(sd);
        CHECK_RC(r, SQLITE_OK);

        r = sqlite3_exec(db, "COMMIT", 0, 0, 0);
        CHECK_RC(r, SQLITE_OK);

        save_from_memory_to_file(f += ".sqlite", db);

        r = sqlite3_close_v2(db);
        CHECK_RC(r, SQLITE_OK);
    }
    LOG_INFO(logger, "save to sqlite db time: " << t2.getTimeFloat() << " s.");

    /*ScopedTime t3;
    {
        BinaryContext b(10'000'000);
        for (auto i = files.getIterator(); i.isValid(); i.next())
        {
            auto &f = *i.getValue();
            if (!f.data)
                continue;

            auto h1 = std::hash<path>()(f.file);
            b.write(h1);
            b.write(normalize_path(f.file));
            b.write(f.data->last_write_time.time_since_epoch().count());
            //b.write(f.data->size);
            b.write(f.implicit_dependencies.size());

            for (auto &[f, d] : f.implicit_dependencies)
                b.write(std::hash<path>()(d->file));
        }
        b.save(f += ".2");
    }
    LOG_INFO(logger, "save to file2 time: " << t3.getTimeFloat() << " s.");*/
}

template <class T>
static void write_int(std::vector<uint8_t> &vec, T val)
{
    auto sz = vec.size();
    vec.resize(sz + sizeof(val));
    memcpy(&vec[sz], &val, sizeof(val));
}

static void write_str(std::vector<uint8_t> &vec, const String &val)
{
    auto sz = val.size();
    write_int(vec, sz);
    auto vsz = vec.size();
    vec.resize(vsz + sz);
    memcpy(&vec[vsz], &val[0], sz);
}

void FileDb::write(std::vector<uint8_t> &v, const FileRecord &f) const
{
    v.clear();

    write_int(v, std::hash<path>()(f.file));
    write_str(v, normalize_path(f.file));
    write_int(v, f.data->last_write_time);
    //write_int(v, f.data->size);
    //write_int(v, f.data->flags.to_ullong());

    auto n = f.implicit_dependencies.size();
    write_int(v, n);

    for (auto &[f, d] : f.implicit_dependencies)
        write_int(v, std::hash<path>()(d->file));
}

void FileDb::load(ConcurrentCommandStorage &commands) const
{
    auto f = getDir() += ".commands";
    FILE *fp = primitives::filesystem::fopen(f, "rb");
    if (!fp)
    {
        if (fs::exists(f))
            throw std::runtime_error("Cannot open file: " + f.u8string());
        return;
    }
    while (!feof(fp))
    {
        int64_t k;
        fread(&k, sizeof(k), 1, fp);
        size_t h;
        fread(&h, sizeof(h), 1, fp);
        commands.insert_ptr(k, h);
    }
    fclose(fp);
}

void FileDb::save(ConcurrentCommandStorage &commands) const
{
    ScopedFile fp(getDir() += ".commands", "wb");
    for (auto i = commands.getIterator(); i.isValid(); i.next())
    {
        int64_t k = (int64_t)i.getKey();
        fwrite(&k, sizeof(k), 1, fp.getHandle());
        auto &h = *i.getValue();
        fwrite(&h, sizeof(h), 1, fp.getHandle());
    }
}

}
