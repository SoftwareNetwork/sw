// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "file.h"

#include "command.h"
#include "file_storage.h"

#include <primitives/executor.h>

#include <fstream>

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
    //if (refreshed == FileData::RefreshType::InProcess)
        //refreshed = FileData::RefreshType::Unrefreshed;

    return *this;
}

void FileData::reset()
{
    generator.reset();
    refreshed = FileData::RefreshType::Unrefreshed;
}

File::File(const path &p, FileStorage &fs)
    : file(p)
{
    if (file.empty())
        throw SW_RUNTIME_ERROR("Empty file");
    data = &fs.registerFile(file);
}

path File::getPath() const
{
    return file;
}

FileData &File::getFileData()
{
    return *data;
}

const FileData &File::getFileData() const
{
    return *data;
}

void FileData::refresh(const path &file)
{
    FileData::RefreshType r = FileData::RefreshType::Unrefreshed;
    if (!refreshed.compare_exchange_strong(r, FileData::RefreshType::InProcess))
        return;

    // extra protection
    // some files throw before the last line :(
    /*SCOPE_EXIT
    {
        if (refreshed == FileData::RefreshType::InProcess)
            refreshed = FileData::RefreshType::Unrefreshed;
    };*/

    bool changed = false;
    auto s = fs::status(file);
    if (s.type() != fs::file_type::regular)
    {
        if (s.type() != fs::file_type::not_found)
            LOG_TRACE(logger, "checking for non-regular file: " << file);
        // we skip non regular files at the moment
        last_write_time = fs::file_time_type::min();
        changed = true;
    }
    else
    {
        auto t = fs::last_write_time(file);
        if (t > last_write_time)
        {
            last_write_time = t;
            changed = true;
        }
    }

    refreshed = changed ? FileData::RefreshType::Changed : FileData::RefreshType::NotChanged;
}

bool File::isChanged() const
{
    while (data->refreshed < FileData::RefreshType::NotChanged)
        data->refresh(file);
    return data->refreshed == FileData::RefreshType::Changed;
}

std::optional<String> File::isChanged(const fs::file_time_type &in, bool throw_on_missing)
{
    isChanged();
    if (data->last_write_time == fs::file_time_type::min())
        return "file is missing";
    if (data->last_write_time > in)
    {
        // if you see equal times after conversion to time_t,
        // it means that lwt resolution is higher
        return "file is newer than command time (" +
            std::to_string(file_time_type2time_t(data->last_write_time)) + " > " +
            std::to_string(file_time_type2time_t(in)) + ")";
    }
    return {};
}

bool File::isGenerated() const
{
    return !!data->generator.lock();
}

bool File::isGeneratedAtAll() const
{
    return data->generated;
}

void File::setGenerated(bool g)
{
    data->generated = g;
}

void File::setGenerator(const std::shared_ptr<builder::Command> &g, bool ignore_errors)
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

std::shared_ptr<builder::Command> File::getGenerator() const
{
    return data->generator.lock();
}

}
