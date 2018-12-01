// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "file_storage.h"

#include "db.h"

#include <primitives/debug.h>
#include <primitives/file_monitor.h>
#include <primitives/sw/settings.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "file_storage");

cl::opt<bool> useFileMonitor("use-file-monitor", cl::init(true));

namespace sw
{

static Executor async_executor("async log writer", 1);

primitives::filesystem::FileMonitor &getFileMonitor()
{
    static primitives::filesystem::FileMonitor fm;
    return fm;
}

FileStorage::file_holder::file_holder(const path &fn)
    : f(fn, "ab")
{
    // goes first
    // but maybe remove?
    //if (setvbuf(f.getHandle(), NULL, _IONBF, 0) != 0)
        //throw std::runtime_error("Cannot disable log buffering");

    // Opening a file in append mode doesn't set the file pointer to the file's
    // end on Windows. Do that explicitly.
    fseek(f.getHandle(), 0, SEEK_END);
}

FileStorage::file_holder::~file_holder()
{
    error_code ec;
    fs::remove(fn, ec);
}

ConcurrentHashMap<path, FileData> &getFileData()
{
    static ConcurrentHashMap<path, FileData> file_data;
    return file_data;
}

std::map<String, FileStorage> &getFileStorages()
{
    getFileData();

    static std::map<String, FileStorage> fs;
    return fs;
}

FileStorage &getFileStorage(const String &config)
{
    static std::mutex m;
    auto &fs = getFileStorages();
    std::unique_lock lk(m);
    auto i = fs.find(config);
    if (i == fs.end())
        i = fs.emplace(config, config).first;
    return i->second;
}

FileStorage::FileStorage(const String &config)
    : config(config)
{
    load();
}

FileStorage::file_holder *FileStorage::getLog()
{
    if (!async_log)
        async_log = std::make_unique<file_holder>(getFilesLogFileName(config));
    return async_log.get();
}

FileStorage::~FileStorage()
{
    try
    {
        async_log.reset();
        save();
    }
    catch (std::exception &e)
    {
        LOG_ERROR(logger, "Error during file db save: " << e.what());
    }
}

void FileStorage::async_file_log(const FileRecord *r)
{
    static std::vector<uint8_t> v;
    async_executor.push([this, r]
    {
        getDb().write(v, *r);

        //fseek(f.f, 0, SEEK_END);
        auto l = getLog();
        fwrite(&v[0], v.size(), 1, l->f.getHandle());
        fflush(l->f.getHandle());
    });
}

void FileStorage::load()
{
    getDb().load(*this, files);

    for (auto i = files.getIterator(); i.isValid(); i.next())
    {
        auto &f = *i.getValue();
        f.fs = this;
    }
}

void FileStorage::save()
{
    getDb().save(*this, files);
}

void FileStorage::reset()
{
    /*save();
    // we have some vars (files) not dumped or something like this
    // do not remove!
    files.clear();
    load();*/
    for (auto i = files.getIterator(); i.isValid(); i.next())
    {
        auto &f = *i.getValue();
        f.reset();
    }
}

FileRecord *FileStorage::registerFile(const File &in_f)
{
    // fs path hash on windows differs for lower and upper cases
#ifdef CPPAN_OS_WINDOWS
    // very slow
    //((File*)&in_f)->file = boost::to_lower_copy(normalize_path(in_f.file));
    ((File*)&in_f)->file = normalize_path(in_f.file);
#endif

    auto d = getFileData().insert(in_f.file);
    auto r = files.insert(in_f.file);
    if (r.second)
    {
        //r.first->load(in_f.file);
        /*if (!in_f.file.empty())
            r.first->data->file = in_f.file;*/
    }
    //else
        //r.first->isChanged();
    in_f.r = r.first;
    r.first->data = d.first;
    r.first->fs = this;
    //if (d.second || d.first->last_write_time.time_since_epoch().count() == 0)
        //r.first->load(in_f.file);

    /*if (d.second)
    {
        DEBUG_BREAK_IF_PATH_HAS(in_f.file, "sdir/bison/src/system.h");
        r.first->load(in_f.file);
    }*/

    if (useFileMonitor)
    {
        getFileMonitor().addFile(in_f.file, [this](const path &f)
        {
            auto &r = File(f, *this).getFileRecord();
            error_code ec;
            if (fs::exists(r.file, ec))
                r.data->last_write_time = fs::last_write_time(f);
            else
                r.data->refreshed = false;
        });
    }

    return r.first;
}

FileRecord *FileStorage::registerFile(const path &in_f)
{
    auto p = normalize_path(in_f);
    auto r = files.insert(p);
    r.first->fs = this;
    auto d = getFileData().insert(p);
    r.first->data = d.first;
    //if (!d.first)
        //throw std::runtime_error("Cannot create file data for file: " + in_f.u8string());
    return r.first;
}

}
