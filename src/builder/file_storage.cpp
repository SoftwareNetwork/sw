// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "file_storage.h"

#include "db.h"

#include <primitives/debug.h>
#include <primitives/file_monitor.h>
#include <primitives/sw/cl.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "file_storage");

cl::opt<bool> useFileMonitor("use-file-monitor");// , cl::init(true));

namespace sw
{

SW_DEFINE_GLOBAL_STATIC_FUNCTION(Executor, getFileStorageExecutor)

SW_DEFINE_GLOBAL_STATIC_FUNCTION(primitives::filesystem::FileMonitor, getFileMonitor)
SW_DEFINE_GLOBAL_STATIC_FUNCTION(FileStorages, getFileStorages)
SW_DEFINE_GLOBAL_STATIC_FUNCTION(FileDataHashMap, getFileData)

FileStorage::file_holder::file_holder(const path &fn)
    : f(fn, "ab"), fn(fn)
{
    // goes first
    // but maybe remove?
    //if (setvbuf(f.getHandle(), NULL, _IONBF, 0) != 0)
        //throw RUNTIME_EXCEPTION("Cannot disable log buffering");

    // Opening a file in append mode doesn't set the file pointer to the file's
    // end on Windows. Do that explicitly.
    fseek(f.getHandle(), 0, SEEK_END);
}

FileStorage::file_holder::~file_holder()
{
    f.close();

    error_code ec; // remove ec? but multiple processes may be writing into this log? or not?
    fs::remove(fn, ec);
}

FileStorage &getFileStorage(const String &config, bool local)
{
    static std::mutex m;
    auto &fs = getFileStorages();
    std::unique_lock lk(m);
    auto i = fs.find({ local,config });
    if (i == fs.end())
    {
        i = fs.emplace(std::pair<bool, String>{ local,config }, config).first;
        i->second.fs_local = local;
        i->second.load();
    }
    return i->second;
}

FileStorage &getServiceFileStorage()
{
    return getFileStorage("service", true);
}

FileStorage::FileStorage(const String &config)
    : config(config)
{
    //if (config.empty())
        //throw SW_RUNTIME_ERROR("Empty config");
    //load();
}

FileStorage::file_holder *FileStorage::getFileLog()
{
    if (!async_file_log_)
        async_file_log_ = std::make_unique<file_holder>(getFilesLogFileName(config, fs_local));
    return async_file_log_.get();
}

path getCommandsLogFileName(bool local);

FileStorage::file_holder *FileStorage::getCommandLog(bool local)
{
    if (local)
    {
        if (!async_command_log_local_)
            async_command_log_local_ = std::make_unique<file_holder>(getCommandsLogFileName(local));
        return async_command_log_local_.get();
    }

    if (!async_command_log_)
        async_command_log_ = std::make_unique<file_holder>(getCommandsLogFileName(local));
    return async_command_log_.get();
}

FileStorage::~FileStorage()
{
    try
    {
        closeLogs();
        save();
    }
    catch (std::exception &e)
    {
        LOG_ERROR(logger, "Error during file db save: " << e.what());
    }
}

void FileStorage::closeLogs()
{
    async_file_log_.reset();
    async_command_log_.reset();
    async_command_log_local_.reset();
}

#ifdef _WIN32
#define USE_EXECUTOR 1
#else
#define USE_EXECUTOR 1
#endif

void FileStorage::async_file_log(const FileRecord *r)
{
#if !USE_EXECUTOR
    static std::mutex m;
    std::unique_lock lk(m);
#endif

    static std::vector<uint8_t> v;
#if USE_EXECUTOR
    getFileStorageExecutor().push([this, r = *r] {
#endif
        // write record to vector v
        getDb().write(v, r);

        //fseek(f.f, 0, SEEK_END);
        auto l = getFileLog();
        auto sz = v.size();
        fwrite(&sz, sizeof(sz), 1, l->f.getHandle());
        fwrite(&v[0], sz, 1, l->f.getHandle());
        fflush(l->f.getHandle());
#if USE_EXECUTOR
    });
#endif
}

void FileStorage::async_command_log(size_t hash, size_t lwt, bool local)
{
#if !USE_EXECUTOR
    static std::mutex m;
    std::unique_lock lk(m);
#endif

#if USE_EXECUTOR
    getFileStorageExecutor().push([this, hash, lwt, local] {
#endif
        auto l = getCommandLog(local);
        fwrite(&hash, sizeof(hash), 1, l->f.getHandle());
        fwrite(&lwt, sizeof(lwt), 1, l->f.getHandle());
        fflush(l->f.getHandle());
#if USE_EXECUTOR
    });
#endif
}

void FileStorage::load()
{
    getDb().load(*this, files, fs_local);

    for (auto i = files.getIterator(); i.isValid(); i.next())
    {
        auto &f = *i.getValue();
        f.fs = this;
    }

    //for (auto &[_, f] : files)
        //f->fs = this;
}

void FileStorage::save()
{
    getDb().save(*this, files, fs_local);
}

void FileStorage::clear()
{
    files.clear();
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

    //for (auto &[_, f] : files)
        //f->reset();
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
                r.data->refreshed = FileData::RefreshType::Unrefreshed;
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
        //throw RUNTIME_EXCEPTION("Cannot create file data for file: " + in_f.u8string());
    return r.first;
}

}
