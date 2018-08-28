// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "file_storage.h"

#include "db.h"

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "file_storage");

namespace sw
{

FileDataStorage &getFileDataStorage()
{
    static FileDataStorage fs;
    return fs;
}

FileStorage &getFileStorage(const String &config)
{
    // create first
    getFileDataStorage();

    static std::mutex m;
    static std::map<String, FileStorage> fs;
    std::unique_lock lk(m);
    auto i = fs.find(config);
    if (i == fs.end())
        i = fs.emplace(config, config).first;
    return i->second;
}

FileDataStorage::FileDataStorage()
{
    load();
}

FileDataStorage::~FileDataStorage()
{
    try
    {
        save();
    }
    catch (...)
    {
        LOG_ERROR(logger, "Error during file db save");
    }
}

void FileDataStorage::load()
{
    getDb().load(files);
}

void FileDataStorage::save()
{
    getDb().save(files);
}

FileData *FileDataStorage::registerFile(const File &in_f)
{
    // fs path hash on windows differs for lower and upper cases
#ifdef CPPAN_OS_WINDOWS
    // very slow
    //((File*)&in_f)->file = boost::to_lower_copy(normalize_path(in_f.file));
    ((File*)&in_f)->file = normalize_path(in_f.file);
#endif

    auto r = files.insert(in_f.file);
    if (r.second)
    {
        //r.first->load(in_f.file);
        if (!in_f.file.empty())
            r.first->file = in_f.file;
    }
    //else
        //r.first->isChanged();
    in_f.r->data = r.first;
    return r.first;
}

FileData *FileDataStorage::registerFile(const path &in_f)
{
    auto r = files.insert(in_f);
    if (r.second)
    {
        if (!in_f.empty())
            r.first->file = in_f;
    }
    return r.first;
}

FileStorage::FileStorage(const String &config)
    : config(config)
{
    load();
}

FileStorage::~FileStorage()
{
    try
    {
        save();
    }
    catch (...)
    {
        LOG_ERROR(logger, "Error during file db save");
    }
}

void FileStorage::load()
{
    getDb().load(config, files);

    for (auto i = files.getIterator(); i.isValid(); i.next())
    {
        auto &f = *i.getValue();
        f.fs = this;
    }
}

void FileStorage::save()
{
    getDb().save(config, files);
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

    r.first->fs = this;
    return r.first;
}

FileRecord *FileStorage::registerFile(const path &in_f)
{
    auto r = files.insert(in_f);
    r.first->fs = this;
    return r.first;
}

}
