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

#define FILE_DB_FORMAT_VERSION 1
#define COMMAND_DB_FORMAT_VERSION 1

namespace sw
{

static path getDir()
{
    return getUserDirectories().storage_dir_tmp / "db";
}

static path getFilesDbFilename(const String &config)
{
    auto p = getDir();
    p += ".";
    p += std::to_string(FILE_DB_FORMAT_VERSION);
    p += "." + config + ".files";
    return p;
}

static path getCommandsDbFilename()
{
    auto p = getDir();
    p += ".";
    p += std::to_string(COMMAND_DB_FORMAT_VERSION);
    p += ".commands";
    return p;
}

static void load(FileStorage &fs, const path &fn, ConcurrentHashMap<path, FileRecord> &files, std::unordered_map<int64_t, std::unordered_set<int64_t>> &deps)
{
    ScopedShareableFileLock lk(fn);

    BinaryContext b;
    try
    {
        b.load(fn);
    }
    catch (std::exception &)
    {
        if (fs::exists(fn))
            throw;
        return;
    }
    while (!b.eof())
    {
        size_t h;
        b.read(h);

        String p;
        b.read(p);

        auto kv = files.insert(h);
        kv.first->file = p;
        kv.first->data = fs.registerFile(p)->data;

        decltype(kv.first->data->last_write_time) lwt;
        b.read(lwt);

        if (kv.first->data->last_write_time < lwt)
        {
            kv.first->data->last_write_time = lwt;
        }

        size_t n;
        b.read(n);

        for (int i = 0; i < n; i++)
        {
            size_t h2;
            b.read(h2);
            deps[h].insert(h2);
        }
    }
}

static void load_log(FileStorage &fs, const path &fn, ConcurrentHashMap<path, FileRecord> &files, std::unordered_map<int64_t, std::unordered_set<int64_t>> &deps)
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

    sw::load(fs, getFilesDbFilename(fs.config), files, deps);
    sw::load_log(fs, getFilesLogFileName(fs.config), files, deps);
    error_code ec;
    fs::remove(getFilesLogFileName(fs.config), ec);

    for (auto &[k, v] : deps)
    {
        for (auto &h2 : v)
        {
            if (!h2)
                continue;
            auto k2 = &files[h2];
            if (k2 && !k2->file.empty())
                files[k].implicit_dependencies.insert({ k2->file, k2 });
        }
    }
}

void FileDb::save(FileStorage &fs, ConcurrentHashMap<path, FileRecord> &files) const
{
    const auto f = getFilesDbFilename(fs.config);

    // first, we load current copy of files
    ConcurrentHashMap<path, FileRecord> old;
    load(fs, old);

    // compare and renew our (actually any) copy
    for (auto i = old.getIterator(); i.isValid(); i.next())
    {
        auto &f = *i.getValue();
        if (f.file.empty())
            continue;
        auto[ptr, inserted] = files.insert(f.file, f);
        if (!inserted && f.data && *ptr < f)
            *ptr = f;
    }

    BinaryContext b(10'000'000); // reserve amount
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
    b.save(f);
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
    const auto fn = getCommandsDbFilename();
    BinaryContext b;
    try
    {
        b.load(fn);
    }
    catch (std::exception &)
    {
        if (fs::exists(fn))
            throw;
        return;
    }
    while (!b.eof())
    {
        size_t k;
        b.read(k);
        size_t h;
        b.read(h);
        commands.insert_ptr(k, h);
    }
}

void FileDb::save(ConcurrentCommandStorage &commands) const
{
    BinaryContext b(10'000'000); // reserve amount
    for (auto i = commands.getIterator(); i.isValid(); i.next())
    {
        b.write(i.getKey());
        b.write(*i.getValue());
    }
    b.save(getCommandsDbFilename());
}

}
