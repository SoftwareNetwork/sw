// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "db_file.h"

#include <directories.h>
//#include <target.h>

#include <primitives/debug.h>
#include <primitives/lock.h>

namespace sw
{

static path getDir()
{
    return getUserDirectories().storage_dir_tmp / "db";
}

static void load(const path &fn, ConcurrentHashMap<path, FileRecord> &files, std::unordered_map<int64_t, std::unordered_set<int64_t>> &deps)
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

        decltype(kv.first->last_write_time) lwt;
        fread(&lwt, sizeof(kv.first->last_write_time), 1, fp);

        sz = 0;
        fread(&sz, sizeof(kv.first->size), 1, fp);

        if (kv.first->last_write_time < lwt)
        {
            kv.first->last_write_time = lwt;
            kv.first->size = sz;
        }

        uint64_t flags;
        fread(&flags, sizeof(flags), 1, fp);
        kv.first->flags = flags;

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

void FileDb::load(const String &config, ConcurrentHashMap<path, FileRecord> &files) const
{
    std::unordered_map<int64_t, std::unordered_set<int64_t>> deps;

    sw::load(getDir() += "." + config + ".files", files, deps);
    sw::load(getFilesLogFileName(config), files, deps);
    error_code ec;
    fs::remove(getFilesLogFileName(config), ec);

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

void FileDb::save(const String &config, ConcurrentHashMap<path, FileRecord> &files) const
{
    const auto f = getDir() += "." + config + ".files";

    // first, we load current copy of files
    ConcurrentHashMap<path, FileRecord> old;
    load(config, old);

    // compare and renew our (actually any) copy
    for (auto i = old.getIterator(); i.isValid(); i.next())
    {
        auto &f = *i.getValue();
        auto[ptr, inserted] = files.insert(f.file, f);
        if (!inserted && *ptr < f)
            *ptr = f;
    }

    // lock and write
    ScopedFileLock lk(f);

    ScopedFile fp(f, "wb");
    std::vector<uint8_t> v;
    for (auto i = files.getIterator(); i.isValid(); i.next())
    {
        auto &f = *i.getValue();

        //DEBUG_BREAK_IF_PATH_HAS(f.file, "gettext-runtime/intl/vasnprintf.c");

        f.isChanged(false); // update file info
        write(v, f);
        fwrite(&v[0], v.size(), 1, fp.getHandle());
    }
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
    write_int(v, f.last_write_time);
    write_int(v, f.size);
    write_int(v, f.flags.to_ullong());

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
