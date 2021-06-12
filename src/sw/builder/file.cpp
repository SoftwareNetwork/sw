// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2017-2020 Egor Pugin

#include "file.h"

#include "command.h"
#include "file_storage.h"

#include <sw/manager/settings.h>

#include <primitives/executor.h>

#include <fstream>
#include <sstream>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "file");

#define SW_EXPLAIN_FILE ".sw/misc/explain.txt"

namespace sw
{

void explainMessage(const String &subject, bool outdated, const String &reason, const String &name)
{
    static Executor explain_executor("explain executor", 1);
    static std::ofstream o([]()
    {
        fs::create_directories(path(SW_EXPLAIN_FILE).parent_path());
        return SW_EXPLAIN_FILE;
    }()); // goes first
    explain_executor.push([=]
    {
        if (!outdated)
            return;
        auto print = [&subject, &name, &reason](auto &o)
        {
            o << subject << ": " << name << "\n";
            o << "outdated\n";
            o << "reason = " << reason << "\n" << std::endl;
        };
        print(o);
        if (sw::Settings::get_user_settings().gExplainOutdatedToTrace)
        {
            std::ostringstream ss;
            print(ss);
            LOG_TRACE(logger, ss.str());
        }
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

    // if we copy data during refresh() we get bad state
    // FIXME: later we must delete file data
    if (refreshed == FileData::RefreshType::InProcess)
        refreshed = FileData::RefreshType::Unrefreshed;

    return *this;
}

void FileData::reset()
{
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

}
