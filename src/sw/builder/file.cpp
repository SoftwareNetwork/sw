// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "file.h"

#include "command.h"
#include "concurrent_map.h"
#include "file_storage.h"

#include <sw/manager/settings.h>
#include <sw/support/hash.h>

#include <boost/algorithm/string.hpp>
#include <boost/dll.hpp>
#include <primitives/executor.h>
#include <primitives/hash.h>
#include <primitives/templates.h>
#include <primitives/debug.h>
#include <primitives/sw/settings.h>

#include <sstream>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "file");

#define CPPAN_FILES_EXPLAIN_FILE ".sw/misc/explain.txt"

namespace sw
{

void explainMessage(const String &subject, bool outdated, const String &reason, const String &name)
{
    static Executor explain_executor("explain executor", 1);
    static std::ofstream o([]()
    {
        fs::create_directories(path(CPPAN_FILES_EXPLAIN_FILE).parent_path());
        return CPPAN_FILES_EXPLAIN_FILE;
    }()); // goes first
    explain_executor.push([=]
    {
        if (!outdated)
            return;
        o << subject << ": " << name << "\n";
        o << "outdated\n";
        o << "reason = " << reason << "\n" << std::endl;
    });
}

File::File(const path &p, FileStorage &s)
    : fs(&s), file(p)
{
    if (file.empty())
        throw SW_RUNTIME_ERROR("Empty file");
    if (!fs)
        throw SW_RUNTIME_ERROR("Empty file storage when loading file: " + normalize_path(file));
    registerSelf();
    r->setFile(file);
}

File &File::operator=(const path &rhs)
{
    file = rhs;
    registerSelf();
    return *this;
}

void File::registerSelf() const
{
    if (r)
        return;
    fs->registerFile(*this);
}

path File::getPath() const
{
    return file;
}

FileRecord &File::getFileRecord()
{
    registerSelf();
    return *r;
}

const FileRecord &File::getFileRecord() const
{
    return ((File*)this)->getFileRecord();
}

bool File::isChanged() const
{
    registerSelf();
    return r->isChanged();
}

std::optional<String> File::isChanged(const fs::file_time_type &t, bool throw_on_missing)
{
    return getFileRecord().isChanged(t, throw_on_missing);
}

bool File::isGenerated() const
{
    registerSelf();
    return r->isGenerated();
}

bool File::isGeneratedAtAll() const
{
    registerSelf();
    return r->isGeneratedAtAll();
}

FileData::FileData(const FileData &rhs)
{
    *this = rhs;
}

FileData &FileData::operator=(const FileData &rhs)
{
    last_write_time = rhs.last_write_time;
    //size = rhs.size;
    //hash = rhs.hash;
    //flags = rhs.flags;

    refreshed = rhs.refreshed.load();

    return *this;
}

FileRecord::FileRecord(const FileRecord &rhs)
{
    operator=(rhs);
}

FileRecord &FileRecord::operator=(const FileRecord &rhs)
{
    file = rhs.file;
    data = rhs.data;
    return *this;
}

void FileRecord::setFile(const path &p)
{
    if (file.empty())
    {
        std::unique_lock lk(m);
        if (file.empty())
            file = p;
    }
}

void FileRecord::reset()
{
    data->generator.reset();
    if (data)
        data->refreshed = FileData::RefreshType::Unrefreshed;
}

void FileRecord::refresh()
{
    if (data->refreshed >= FileData::RefreshType::NotChanged)
        return;

    std::unique_lock lk(m);

    if (data->refreshed >= FileData::RefreshType::NotChanged)
        return;

    /*FileData::RefreshType r = FileData::RefreshType::Unrefreshed;
    if (!data || !data->refreshed.compare_exchange_strong(r, FileData::RefreshType::InProcess))
        return;*/

    bool changed = false;
    auto s = fs::status(file);
    if (s.type() != fs::file_type::regular)
    {
        if (s.type() != fs::file_type::not_found)
            LOG_TRACE(logger, "checking for non-regular file: " << file);
        // we skip non regular files at the moment
        data->last_write_time = decltype(data->last_write_time)();
        changed = true;
    }
    else
    {
        auto t = fs::last_write_time(file);
        if (t > data->last_write_time)
        {
            data->last_write_time = t;
            changed = true;
        }
    }

    data->refreshed = changed ? FileData::RefreshType::Changed : FileData::RefreshType::NotChanged;
}

bool FileRecord::isChanged()
{
    refresh();

    // spin
    //while (data->refreshed < FileData::RefreshType::NotChanged)
        //;

    return data->refreshed == FileData::RefreshType::Changed;
}

std::optional<String> FileRecord::isChanged(const fs::file_time_type &in, bool throw_on_missing)
{
    // we call this as refresh of all deps
    // explain inside
    isChanged();

    // on missing direct file we fail immediately
    if (data->last_write_time.time_since_epoch().count() == 0)
    {
        if (throw_on_missing)
            throw SW_RUNTIME_ERROR("file " + normalize_path(file) + " is missing");
        return "file is missing";
    }

    if (data->last_write_time > in)
    {
        return "file is newer";
    }
    return {};
}

void FileRecord::setGenerator(const std::shared_ptr<builder::Command> &g, bool ignore_errors)
{
    if (!g)
        return;

    auto gold = data->generator.lock();
    if (!ignore_errors && gold && (gold != g &&
        !gold->isExecuted() &&
        !gold->maybe_unused &&
        gold->getHash() != g->getHash()))
    {
        String err;
        err += "Setting generator twice on file: " + file.u8string() + "\n";
        if (gold)
        {
            err += "first generator:\n " + gold->print() + "\n";
            err += "first generator hash:\n " + std::to_string(gold->getHash());
        }
        else
            err += "first generator is empty";
        err += "\n";
        if (g)
        {
            err += "second generator:\n " + g->print() + "\n";
            err += "second generator hash:\n " + std::to_string(g->getHash());
        }
        else
            err += "second generator is empty";
        throw SW_RUNTIME_ERROR(err);
    }
    data->generator = g;
    data->generated = true;
}

std::shared_ptr<builder::Command> FileRecord::getGenerator() const
{
    return data->generator.lock();
}

bool FileRecord::isGenerated() const
{
    return !!data->generator.lock();
}

bool FileRecord::operator<(const FileRecord &r) const
{
    return data->last_write_time < r.data->last_write_time;
}

}
